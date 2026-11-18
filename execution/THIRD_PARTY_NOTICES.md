# Source and dependency references

No third-party implementation source is included in this distribution.

- NVIDIA/stdexec: https://github.com/NVIDIA/stdexec
  - API/source baseline: `7d704575019a1405b2067c5e47265245c55ca66e`
  - License: Apache-2.0 WITH LLVM-exception, subject to the actual pinned upstream license files.
  - Developer guide: https://nvidia.github.io/stdexec/developer/index.html
  - Reference: https://nvidia.github.io/stdexec/reference/index.html
- cameron314/concurrentqueue: https://github.com/cameron314/concurrentqueue
  - Version: v1.0.4 / `6dd38b8a1dbaa7863aa907045f32308a56a6ff5d`
  - Upstream offers the simplified BSD and Boost Software License alternatives; consult its LICENSE.md.
  - Documentation: https://github.com/cameron314/concurrentqueue/blob/master/README.md
- Integration conventions inspected in LUX-YU/lux-cxx:
  - Root CMake blob: `df12ace058542d414480d7b079acb1828f6985c7`
  - concurrent component CMake blob: `80607c57d834cf2cb58c7302d0ac14efc3738a62`
- Existing lux-engine stdexec pin:
  - https://github.com/LUX-YU/lux-engine/blob/codex/render-v2-rebuild/cmake/vcpkg-ports/stdexec/portfile.cmake

The original user-provided design is LUX_Execution_Framework_Design_and_Godbolt_Examples.zip,
README Draft 0.1. The changes from that draft are explicitly listed in CONTRACT.zh-CN.md.
No assertion is made that a moving upstream branch is ABI/API compatible without validation.
