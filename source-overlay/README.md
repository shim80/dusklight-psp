# PSP source overlay

This directory stores the redistributable PSP-specific source/tests/scripts snapshot as bounded Base64 fragments. The previous single-file transport was removed because it was truncated by the connector.

Reconstruct the archive with:

```sh
./source-overlay/reconstruct.sh /tmp/dusklight-psp-code-overlay.tar.xz
mkdir -p /tmp/dusklight-psp-overlay
tar -xJf /tmp/dusklight-psp-code-overlay.tar.xz -C /tmp/dusklight-psp-overlay
```

Canonical decoded archive:

- size: 179556 bytes
- SHA-256: `ce56f4d674c2faad781dfd53ae1ff3a5e7110d29a3ecfab756947c099a582527`
- paths: 204

The archive contains project source/tests/scripts only. It contains no Nintendo game image or extracted commercial game assets.
