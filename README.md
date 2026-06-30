# sproxy
基于SSH隧道的透明代理工具。通过SSH连接建立tunnel/及socks代理，利用nftables tproxy实现透明代理。

## 工作原理

```
本机应用 ─┬─ TCP 流量 → nftables tproxy   → Socks5代理 → SSH隧道 → 远程服务器
          └─ DNS 查询 → nftables redirect → 内置rdns   → 按域名路由上游/回调注入代理IP
```

## 功能特性
- **SSH隧道**：通过libssh2建立SSH隧道，支持SOCKS5代理流量经隧道转发至远程服务器. 远程服务支持SSH登录即可. 
- **透明代理**：利用nftables tproxy转发，应用无感，因此无需应用配置
- **域名路由**：内置dns服务，按域名匹配规则分发到不同上游DNS，支持正则匹配
- **DNS劫持**：nftables将发往系统 nameserver的DNS查询重定向到内置dns服务，无需修改`/etc/resolv.conf`
- **双列表机制**：支持goproxy与bypass静态配置是否走代理
- **动态IP注入**：根据DNS解析回调，将需要走代理的IP动态加入nftables tproxy转发集合
- **动态重载**：通过 inotify 监听列表文件变化，自动重载并重建规则

### 运行时
- Linux 内核支持 tproxy（`CONFIG_NETFILTER_TPROXY`）
- nftables
- root 权限（nftables 操作需要 `CAP_NET_ADMIN`）
- 远程SSH服务器

### 编译时
- Rust toolchain（edition 2021）
- C编译器（支持 C99）
- libevent（core + extra + openssl）
- libssh2
- openssl
- libnftables

## 编译
```bash
cargo build --release
```


## 配置
通过 YAML 配置文件启动：
```bash
sudo ./sproxy config.yaml
```

### 配置项
| 字段                   | 类型 | 默认值                       | 说明 |
|----------------------|------|---------------------------|------|
| `log.level`          | String | `info`                    | 日志级别：trace/debug/info/warn/error |
| `ssh.host`           | String | —                         | SSH 服务器地址（必填） |
| `ssh.port`           | u16 | `22`                      | SSH 端口 |
| `ssh.user`           | String | `root`                    | SSH 用户名 |
| `ssh.key`            | String | —                         | SSH 私钥路径 |
| `socks5.listen`      | String | `127.0.0.1:1080`          | SOCKS5 监听地址；设为 null 关闭 |
| `tproxy.port`        | u16 | `1081`                    | tproxy 透明代理端口 |
| `dns.listen`         | String | `127.0.0.1:1053`          | rdns 监听地址；设为 null 关闭 DNS 功能 |
| `dns.client`         | u16 | `1054`                    | rdns 自身向上游查询使用的本地端口（用于 nftables 放行） |
| `dns.proxy`          | String | —                         | goproxy 域名上游 DNS，如 `8.8.8.8:53`；不设则按 `servers` 规则查询 |
| `dns.servers`        | List | —                         | DNS 路由规则列表，见下文 |
| `proxy.static.goproxy` | String | `/etc/proxy/goproxy.cidr.txt` | 必须代理的 CIDR 地址列表，加载到 nftables `goproxy` 集合 |
| `proxy.static.bypass`  | String | `/etc/proxy/bypass.cidr.txt`  | 绕过代理的 CIDR 地址列表，加载到 nftables `bypass` 集合 |
| `proxy.auto`           | String | `/etc/proxy/auto.txt`         | 域名列表（proxy/bypass 域名，见下文） |

#### DNS 路由规则

`dns.servers` 中每条规则按顺序匹配，字段如下：

| 字段 | 说明 |
|------|------|
| `match` | 域名匹配模式（正则），`*` 匹配所有 |
| `server` | 上游 DNS 服务器地址，如 `8.8.8.8:53` |
| `proxy` | 是否标记为代理域名（`true` 时解析出的 IP 注入 nftables `auto` 集合） |

### 配置示例

```yaml
log:
  level: info

ssh:
  host: "your-server-ip"
  port: 22
  user: "root"
  key: "/etc/sproxy/proxy.key"

socks5:
  listen: "127.0.0.1:1080"

tproxy:
  port: 1081

dns:
  listen: "127.0.0.1:1053"
  client: 1054
  proxy: "8.8.8.8:53"
  servers:
    - match: "*.cn"
      server: "223.5.5.5:53"
    - match: "google.com|github.com|youtube.com"
      server: "8.8.8.8:53"
      proxy: true
    - match: "*"
      server: "8.8.8.8:53"

proxy:
  static:
    goproxy: "/etc/sproxy/goproxy.cidr.txt"
    bypass: "/etc/sproxy/bypass.cidr.txt"
  auto: "/etc/sproxy/auto.txt"
```

### 地址/域名列表格式

三类列表文件，每行一条，`#` 开头为注释，空行忽略：

| 文件 | 内容 | 说明 |
|------|------|------|
| `goproxy.cidr.txt` | IP/CIDR | 必须走代理的IP段，加载到 nftables `goproxy` 集合 |
| `bypass.cidr.txt` | IP/CIDR | 绕过代理直连的IP段，加载到 nftables `bypass` 集合 |
| `auto.txt` | 域名 | 域名列表，DNS解析命中时注入IP到 nftables `auto` 集合 |

域名列表（`auto.txt`）按行首前缀区分 proxy / bypass：
- `@@` 开头：bypass 域名（DNS 解析时不注入代理，即使 `servers` 规则标记 `proxy: true`）
- `||` 或 `|` 开头：proxy 域名
- 无前缀：默认视为 proxy 域名
- `!`、`[`、`#` 开头为注释，`/regex/` 形式不支持，均忽略

域名匹配支持子域名：若列表中有 `google.com`，则 `www.google.com` 同样匹配。


## systemd 服务
附带 `proxy.service`，可部署为系统服务：

```bash
sudo cp target/release/sproxy /usr/local/bin/sproxy
sudo cp config.yaml /etc/sproxy/sproxy.yaml
sudo cp proxy.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now proxy
```