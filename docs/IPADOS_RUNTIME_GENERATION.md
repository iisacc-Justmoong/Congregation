# iPadOS runtime generation integrity

The iPad kit stages iiSDKCore before the file, network, canvas and GUI modules.
These modules exchange C++ values such as `ii::runtime::Submission`; they must
therefore be compiled against the same installed runtime ABI. Replacing only
`libiiSDKCore.a` can leave a successfully linked application with incompatible
object layouts in a dependent static library.

`tools/ipados_sdk_coherence.py` records SHA-256 fingerprints of the staged
runtime headers and SDK archives after each successful build stage. Each record
includes that stage and its preceding dependency groups. Starting a rebuild
invalidates the affected stage and later records; the application builder rejects
a missing or inconsistent record before replacing the previous bundle. This
checks the staged artifacts, not the validity of runtime behavior on a device.

When the runtime changes, rebuild in dependency order:

```sh
python3 -B tools/build_ipados_sdk.py base
python3 -B tools/build_ipados_sdk.py files
python3 -B tools/build_ipados_sdk.py network
python3 -B tools/build_ipados_sdk.py canvas
python3 -B tools/build_ipados_sdk.py gui
python3 -B tools/build_ipados.py --device DEVICE_ID --install
```

`tests/test_ipados_sdk_coherence.py` covers changed runtime and dependency
artifacts and interrupted stages. `tests/test_ipados_builder.py` verifies that a
failed coherence check does not remove the prior application bundle.

On 2026-11-08, device crash reports showed `EXC_BAD_ACCESS` in the old
`iiNetwork::Udp::start_receiving()` after a call to the current
`iiSDKCore::EventLoop::every()`. The four runtime workers were waiting on a
condition variable in the captured report; the fault occurred on the main
thread. The old network archive predated the installed Core runtime and its
call-site assembly allocated a smaller return area than the current runtime
writes, corrupting an adjacent callback on the stack.

The dependency-ordered rebuild and `Congregation` iPhoneOS arm64 Release build
were completed on 2026-11-08. The signed application was installed over the
existing iPad app and launched normally with its existing preferences. The
application remained present in the device process list beyond the earlier
one-second failure, and no new Congregation crash report appeared. A 15-second
system-wide Time Profiler capture recorded the app's UIKit host loop, Core
`HostLoop` monitor and iiFileProvider worker; it sampled only running threads,
so idle worker health and individual network exchanges were not established by
that recording. A debugger attachment attempt did not obtain a stopped process
and terminated that run with signal 9; the application was subsequently launched
normally and left running. Screen content and touch interaction were not
independently validated in this run.
