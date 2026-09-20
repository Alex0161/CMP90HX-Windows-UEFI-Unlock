# Third-Party Notices / Уведомления о сторонних компонентах

## NVPermissive core

Component: `nvpermissive-core.o`

Observed SHA-256:

`c9702b4887d397272f86dcc25eea2bb11a46d636c91311d7b71f2fb8b01951e5`

Pinned upstream archive:

`nvpermissive-dist-380bdf3.tar.gz`

SHA-256:

`d5e89e77e121e1295cb39d331e0d511275b6edc1d7236376e0fd13ac6f0c79c0`

Upstream location:

https://alist.homelabproject.cc/foxipan/vGPU/CMP_90HX/GraphicsUnlock

Original package attribution: GreenDamTan / RainCandyTech, NVPermissive.

The Windows UEFI wrapper and its new menu code are separate from the
NVPermissive core. The wrapper's MIT license does not automatically relicense
the core.

The pinned core archive reviewed during development did not contain a
standalone license file for `nvpermissive-core.o`. An upstream Linux wrapper
declares `MODULE_LICENSE("GPL")`, but that declaration alone is not treated
here as a complete redistribution license for the standalone object.

For this reason, the public source tree does not include the core object.
Users must supply it separately unless and until its redistribution terms are
confirmed.

## Other projects and references

See `SOURCES_AND_LICENSES_RU_EN.md` for:

- WildFlash1st/cmp90hx-unlock-for-windows
- amoghmunikote/cmpunlocker
- WebForks/cmpunlocker
- xrip/cmp50hx-unlock
- bendy2/cmp90hx
- d3dx9/cmpunlocker
- NVIDIA Open GPU Kernel Modules
- TianoCore EDK II
- Rhonstin/beellama-cmp90hx
- ggml-org/llama.cpp
- Jon Pry's related research publication

Third-party components retain their original licenses and notices.
