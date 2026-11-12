# 发行备用镜像 / Release fallback mirror

## 简体中文

客户端优先使用现有 GitHub、代理和 CNB 线路；OSS 始终最后尝试，不因测速较快而提前。下载仍验证独立 RSA 签名、文件大小与 SHA-256。官网的备用入口也不替换默认下载。

每次经批准发布后，将同一份 EXE、ZIP、`SHA256SUMS.txt` 和签名更新清单镜像到版本目录。版本目录禁止不同字节覆盖；上传后匿名下载逐文件核对。正式版验证完成后才更新 latest 指针，预发布不改正式 latest。

运行工具前执行 `npm ci --prefix tools/release --ignore-scripts`。可先进行完全离线、不读取凭据、不上传的检查：

```powershell
node scripts/publish-oss-release.cjs --version=0.2.3 --directory=delivery/stable-0.2.3 --verify-only=true
```

获准发布后，通过受保护的本机记录或发布环境注入 `OSS_RELEASE_BUCKET`、`OSS_RELEASE_REGION`、`OSS_RELEASE_PUBLIC_BASE`、`OSS_ACCESS_KEY_ID`、`OSS_ACCESS_KEY_SECRET`；临时凭据另注入 `OSS_SECURITY_TOKEN`。不要把实际值写进源码、命令行、仓库配置、日志或发行包。客户端配置仅包含必要的公开只读下载地址。

```powershell
node scripts/publish-oss-release.cjs --version=0.2.3 --directory=delivery/stable-0.2.3 --latest=true
```

账号及上传权限与公开下载分开；发布身份仅授予发行对象前缀所需权限。私钥只用于本地签名，不上传。保留上一正式版，回滚通过明确恢复 latest 指针完成，不篡改版本文件。

## English

Clients try the existing GitHub, proxy and CNB routes first. OSS is always the final fallback, even when its latency is lower. Downloads still require an independent RSA signature, expected size and SHA-256. The website's backup link does not replace its default download.

After an approved release, mirror the identical EXE, ZIP, `SHA256SUMS.txt` and signed update manifest to the versioned directory. Different bytes cannot overwrite an existing version. Verify each object by anonymous download before updating stable latest pointers. Prereleases never replace stable latest.

Install the operator tool with `npm ci --prefix tools/release --ignore-scripts`. The `--verify-only=true` command above validates local files and the signature without reading credentials or uploading.

For an approved upload, inject the named environment variables above from a protected local record or release environment; add `OSS_SECURITY_TOKEN` for temporary credentials. Never put their actual values in source, command-line arguments, repository configuration, logs or release packages. The client contains only necessary public read-only download addresses.

Keep the upload identity separate from anonymous downloads and restrict it to the release object prefix. Signing private keys remain local. Preserve previous versions; rollback restores latest pointers without modifying versioned files.
