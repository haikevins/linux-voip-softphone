Được. Với mục tiêu **không chỉ làm chạy mà còn chứng minh với nhà tuyển dụng rằng bạn thực sự hiểu C++/Linux/Networking/SIP/RTP/Qt/Debugging**, tôi khuyên biến project thành một **engineering portfolio**, không phải chỉ là một app softphone.

Tên repo có thể là:

```text
linux-voip-softphone
```

Mục tiêu cuối:

```text
Ubuntu ThinkPad
│
├── Qt/QML Softphone
│   ├── C++ Core
│   ├── PJSIP
│   ├── RTP Audio
│   ├── Audio Device Manager
│   ├── Network Statistics
│   └── Diagnostics
│
├── Asterisk SIP Server
│
├── Linux system labs
│   ├── TCP/UDP
│   ├── threads
│   ├── IPC
│   ├── signals
│   └── crash/core dump
│
└── Wireshark / GDB / Sanitizers
           │
           │ Wi-Fi
           ▼
      Redmi 12 5G
      SIP endpoint
```

Tôi sẽ thiết kế theo **11 phase**, khoảng 8–12 tuần nếu làm nghiêm túc.

---

# 1. Cấu trúc repository hoàn chỉnh

Tôi đề xuất cấu trúc này:

```text
linux-voip-softphone/
│
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── .gitignore
├── .clang-format
├── .clang-tidy
│
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   ├── StaticAnalysis.cmake
│   └── Dependencies.cmake
│
├── config/
│   ├── app.example.ini
│   ├── logging.example.ini
│   └── asterisk/
│       ├── pjsip.conf.example
│       └── extensions.conf.example
│
├── src/
│   │
│   ├── app/
│   │   ├── main.cpp
│   │   ├── Application.cpp
│   │   └── Application.hpp
│   │
│   ├── core/
│   │   ├── CallManager.cpp
│   │   ├── CallManager.hpp
│   │   ├── CallState.cpp
│   │   ├── CallState.hpp
│   │   ├── Event.hpp
│   │   └── Types.hpp
│   │
│   ├── sip/
│   │   ├── SipEngine.hpp
│   │   ├── PjsipEngine.cpp
│   │   ├── PjsipEngine.hpp
│   │   ├── SipAccount.cpp
│   │   ├── SipAccount.hpp
│   │   ├── SipCall.cpp
│   │   └── SipCall.hpp
│   │
│   ├── media/
│   │   ├── AudioDevice.cpp
│   │   ├── AudioDevice.hpp
│   │   ├── AudioDeviceManager.cpp
│   │   ├── AudioDeviceManager.hpp
│   │   ├── AudioStats.cpp
│   │   └── AudioStats.hpp
│   │
│   ├── network/
│   │   ├── UdpSocket.cpp
│   │   ├── UdpSocket.hpp
│   │   ├── TcpSocket.cpp
│   │   ├── TcpSocket.hpp
│   │   ├── Packet.hpp
│   │   ├── PacketStatistics.cpp
│   │   ├── PacketStatistics.hpp
│   │   ├── RtpStatistics.cpp
│   │   └── RtpStatistics.hpp
│   │
│   ├── threading/
│   │   ├── ThreadSafeQueue.hpp
│   │   ├── WorkerThread.cpp
│   │   └── WorkerThread.hpp
│   │
│   ├── ipc/
│   │   ├── UnixSocketServer.cpp
│   │   ├── UnixSocketServer.hpp
│   │   ├── UnixSocketClient.cpp
│   │   └── UnixSocketClient.hpp
│   │
│   ├── diagnostics/
│   │   ├── Logger.cpp
│   │   ├── Logger.hpp
│   │   ├── Metrics.cpp
│   │   ├── Metrics.hpp
│   │   ├── CrashHandler.cpp
│   │   └── CrashHandler.hpp
│   │
│   ├── platform/
│   │   └── linux/
│   │       ├── SignalHandler.cpp
│   │       ├── SignalHandler.hpp
│   │       ├── ProcessInfo.cpp
│   │       └── ProcessInfo.hpp
│   │
│   └── ui/
│       ├── UiController.cpp
│       ├── UiController.hpp
│       ├── CallViewModel.cpp
│       ├── CallViewModel.hpp
│       ├── AccountViewModel.cpp
│       └── AccountViewModel.hpp
│
├── qml/
│   ├── Main.qml
│   │
│   ├── pages/
│   │   ├── AccountPage.qml
│   │   ├── DialerPage.qml
│   │   ├── IncomingCallPage.qml
│   │   ├── ActiveCallPage.qml
│   │   ├── SettingsPage.qml
│   │   └── DiagnosticsPage.qml
│   │
│   └── components/
│       ├── DialButton.qml
│       ├── CallButton.qml
│       ├── StatusBadge.qml
│       └── AudioLevel.qml
│
├── apps/
│   │
│   ├── sip-cli/
│   │   └── main.cpp
│   │
│   ├── network-monitor/
│   │   └── main.cpp
│   │
│   └── voip-gui/
│       └── main.cpp
│
├── labs/
│   │
│   ├── cpp/
│   │   ├── raii/
│   │   ├── smart-pointers/
│   │   └── move-semantics/
│   │
│   ├── threading/
│   │   ├── mutex/
│   │   ├── condition-variable/
│   │   ├── producer-consumer/
│   │   └── race-condition/
│   │
│   ├── linux/
│   │   ├── signals/
│   │   ├── process/
│   │   ├── unix-domain-socket/
│   │   └── core-dump/
│   │
│   ├── networking/
│   │   ├── tcp-echo/
│   │   ├── udp-echo/
│   │   └── packet-simulator/
│   │
│   └── rtsp/
│       └── rtsp-probe/
│
├── tests/
│   │
│   ├── unit/
│   │   ├── test_call_state.cpp
│   │   ├── test_packet_statistics.cpp
│   │   ├── test_thread_safe_queue.cpp
│   │   └── test_config.cpp
│   │
│   ├── integration/
│   │   ├── test_udp_transport.cpp
│   │   └── test_sip_registration.cpp
│   │
│   └── stress/
│       ├── thread_queue_stress.cpp
│       └── call_state_stress.cpp
│
├── tools/
│   ├── udp_sender/
│   ├── udp_receiver/
│   ├── packet_generator/
│   ├── crash_generator/
│   └── ipc_monitor/
│
├── scripts/
│   ├── bootstrap.sh
│   ├── build.sh
│   ├── test.sh
│   ├── run_sanitizers.sh
│   ├── run_clang_tidy.sh
│   ├── collect_logs.sh
│   └── capture_sip.sh
│
├── docs/
│   │
│   ├── architecture/
│   │   ├── overview.md
│   │   ├── threading-model.md
│   │   ├── call-state-machine.md
│   │   └── component-diagram.md
│   │
│   ├── protocols/
│   │   ├── sip.md
│   │   ├── rtp.md
│   │   ├── sdp.md
│   │   └── rtsp.md
│   │
│   ├── debugging/
│   │   ├── gdb.md
│   │   ├── core-dump-analysis.md
│   │   ├── asan-analysis.md
│   │   └── race-condition-analysis.md
│   │
│   ├── adr/
│   │   ├── ADR-001-pjsip.md
│   │   ├── ADR-002-thread-model.md
│   │   ├── ADR-003-state-machine.md
│   │   └── ADR-004-ipc.md
│   │
│   └── interview/
│       ├── project-overview.md
│       └── questions-and-answers.md
│
├── evidence/
│   │
│   ├── 01-cpp/
│   ├── 02-linux/
│   ├── 03-networking/
│   ├── 04-sip/
│   ├── 05-rtp/
│   ├── 06-qt/
│   ├── 07-threading/
│   ├── 08-debugging/
│   ├── 09-testing/
│   └── 10-final-demo/
│
├── captures/
│   ├── README.md
│   ├── sip-register.pcapng
│   ├── sip-call.pcapng
│   └── rtp-call.pcapng
│
└── .github/
    └── workflows/
        ├── build.yml
        ├── test.yml
        └── static-analysis.yml
```

Không cần tạo toàn bộ file ngay ngày đầu. Đây là **target structure**; từng phase sẽ dần populate repo.

---

# 2. Tại sao phải có cả `labs/` và `src/`

Đây là một điểm rất quan trọng.

Nếu chỉ có:

```text
src/
```

nhà tuyển dụng thấy app chạy nhưng khó biết bạn có thực sự hiểu:

```text
mutex
condition_variable
UDP
IPC
signals
core dump
```

hay chỉ gọi API PJSIP/Qt.

Vì vậy:

```text
labs/
```

chứa những bài nhỏ mà bạn **tự implement để chứng minh kiến thức nền**.

Còn:

```text
src/
```

chứa production code được áp dụng vào softphone.

Ví dụ:

```text
labs/threading/producer-consumer
```

chứng minh bạn hiểu producer-consumer.

Sau đó:

```text
src/threading/ThreadSafeQueue.hpp
```

chứng minh bạn biết đưa kiến thức đó vào project thật.

Đây chính là kiểu bằng chứng rất mạnh khi phỏng vấn.

---

# 3. Phase 0 — Repository & Engineering Environment

Thời gian:

```text
1–2 ngày
```

Mục tiêu chưa phải VoIP.

Bạn thiết lập:

```text
Git
CMake
C++20
clang-format
clang-tidy
GoogleTest
ASan
UBSan
GitHub Actions
```

Build structure:

```text
source
   ↓
CMake
   ↓
compile
   ↓
unit tests
   ↓
clang-tidy
   ↓
sanitizers
```

Build nên chạy được:

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build -j

ctest --test-dir build
```

### Kết quả phải có

README ban đầu:

```text
Build: Passing
Tests: Passing
Compiler: GCC / Clang
Standard: C++20
Platform: Ubuntu
```

Git tag:

```text
v0.1-project-skeleton
```

### Bằng chứng cho nhà tuyển dụng

Trong repo phải nhìn thấy:

```text
CMakeLists.txt
.clang-format
.clang-tidy
tests/
.github/workflows/
```

Bạn có thể giải thích:

> Tôi thiết lập project với CMake, compiler warnings, static analysis và sanitizer ngay từ đầu để phát hiện lỗi sớm thay vì chỉ debug khi runtime.

---

# 4. Phase 1 — Modern C++ foundation

Thời gian:

```text
4–6 ngày
```

Học và sử dụng thực tế:

```text
RAII
smart pointer
move semantics
const correctness
enum class
std::optional
std::variant
STL containers
interfaces
dependency injection cơ bản
```

Tạo:

```text
labs/cpp/
```

Ví dụ RAII wrapper:

```cpp
class FileDescriptor {
public:
    explicit FileDescriptor(int fd)
        : fd_(fd)
    {
    }

    ~FileDescriptor()
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

private:
    int fd_;
};
```

Sau này socket của bạn dùng đúng nguyên tắc đó.

### Production component

Bắt đầu:

```text
src/core/
```

với:

```cpp
enum class CallState {
    Idle,
    Registering,
    Ready,
    Calling,
    Ringing,
    Connected,
    Terminating,
    Failed
};
```

### Bằng chứng

Tạo:

```text
docs/architecture/call-state-machine.md
```

và unit test:

```text
tests/unit/test_call_state.cpp
```

Ví dụ test các transition không hợp lệ.

Git tag:

```text
v0.2-cpp-core
```

Điểm phỏng vấn bạn phải giải thích được:

```text
unique_ptr vs shared_ptr
RAII giải quyết vấn đề gì
copy vs move
vector vs list
map vs unordered_map
enum class tốt hơn enum ở đâu
```

---

# 5. Phase 2 — Linux System Programming

Thời gian:

```text
5–7 ngày
```

Đây là phase rất quan trọng vì JD có Linux.

Bạn làm các lab riêng:

```text
labs/linux/process
labs/linux/signals
labs/linux/unix-domain-socket
labs/linux/core-dump
```

Học:

```text
fork()
exec()
signal / sigaction()
PID
file descriptor
Unix domain socket
process lifecycle
```

Tạo một process:

```text
network-monitor
```

và cho softphone gửi statistics qua:

```text
Unix Domain Socket
```

Kiến trúc:

```text
VoIP Application
      │
      │ Unix Domain Socket
      ▼
Network Monitor Process
```

Ví dụ message:

```json
{
  "call_id": 42,
  "packets": 1240,
  "lost": 7,
  "jitter_ms": 12
}
```

Điều này giúp bạn chứng minh **IPC**, thay vì chỉ nói “em biết IPC”.

### Evidence

Lưu:

```text
evidence/phase2/
```

với:

```text
process-tree.txt
unix-socket-demo.txt
signal-demo.txt
```

Có thể chụp:

```bash
ps -ef
```

và:

```bash
ss -xl
```

để thấy Unix socket.

Git tag:

```text
v0.3-linux-system
```

---

# 6. Phase 3 — TCP/UDP Network Programming

Thời gian:

```text
5–7 ngày
```

Không dùng PJSIP ở phase này.

Bạn tự viết:

```text
TCP echo server/client
UDP sender/receiver
packet simulator
```

Packet:

```cpp
struct PacketHeader {
    uint32_t sequence;
    uint64_t timestamp;
};
```

Luồng:

```text
UDP Sender
    │
    │ packet #1
    │ packet #2
    │ packet #4
    ▼
UDP Receiver
```

Receiver phát hiện:

```text
missing packet #3
```

Tính:

```text
packets sent
packets received
packet loss
out-of-order packet
latency
jitter
throughput
```

Đây chính là nền tảng để hiểu RTP.

### Acceptance criteria

Ví dụ:

```text
Sent:       10,000
Received:    9,847
Lost:          153
Loss:          1.53%
Avg latency:   4.1 ms
Jitter:        1.7 ms
```

Không quan trọng con số chính xác. Quan trọng là bạn phải hiểu **cách tính**.

### Evidence

```text
evidence/phase3/
├── udp-demo.log
├── tcp-demo.log
├── packet-loss-demo.log
└── screenshots/
```

Wireshark:

```text
captures/udp-test.pcapng
```

Git tag:

```text
v0.4-networking
```

---

# 7. Phase 4 — SIP + Asterisk Lab

Thời gian:

```text
4–6 ngày
```

Bây giờ mới bắt đầu VoIP thực sự.

Topology:

```text
                  ThinkPad
         ┌────────────────────────┐
         │                        │
         │ Asterisk              │
         │                        │
         │ account 1001          │
         │ account 1002          │
         │                        │
         └──────────┬─────────────┘
                    │
               Wi-Fi Router
            ┌───────┴───────┐
            │               │
         ThinkPad         Redmi
         SIP Client       SIP Client
         1001             1002
```

Đầu tiên **chưa dùng softphone tự viết**.

Dùng hai SIP client có sẵn để xác nhận Asterisk hoạt động.

Bạn phải nhìn được trong Wireshark:

```text
REGISTER
401 Unauthorized
REGISTER + Authorization
200 OK
```

Và khi gọi:

```text
INVITE
100 Trying
180 Ringing
200 OK
ACK
RTP
BYE
200 OK
```

Đây là một artifact cực kỳ giá trị.

### Bạn phải tự viết tài liệu

```text
docs/protocols/sip.md
```

Giải thích:

```text
REGISTER dùng để làm gì?
INVITE dùng để làm gì?
ACK khác 200 OK như thế nào?
BYE khác CANCEL thế nào?
SDP nằm ở đâu?
RTP được negotiate thế nào?
```

Không copy tài liệu internet.

Viết theo cách bạn hiểu.

### Evidence quan trọng nhất

```text
captures/sip-register.pcapng
captures/sip-call.pcapng
```

và:

```text
evidence/phase4/
├── register-flow.png
├── call-flow.png
├── wireshark-register.png
├── wireshark-invite.png
└── explanation.md
```

Git tag:

```text
v0.5-sip-asterisk
```

---

# 8. Phase 5 — PJSIP CLI Softphone

Thời gian:

```text
1–2 tuần
```

Đây là milestone quan trọng đầu tiên.

Không Qt.

Không QML.

Chỉ terminal.

```text
$ ./sip-cli

Server: 192.168.1.20
User: 1001

[SIP] Registering...
[SIP] Registered successfully

voip> call 1002

[CALL] Calling 1002
[CALL] Ringing
[CALL] Connected

voip> hangup

[CALL] Terminated
```

Architecture:

```text
CLI
 │
 ▼
CallManager
 │
 ▼
SipEngine interface
 │
 ▼
PjsipEngine
 │
 ▼
PJSIP
```

Điểm quan trọng: **đừng cho `CallManager` phụ thuộc trực tiếp vào PJSIP**.

Dùng interface:

```cpp
class SipEngine {
public:
    virtual ~SipEngine() = default;

    virtual bool registerAccount() = 0;
    virtual void makeCall(const std::string& destination) = 0;
    virtual void hangup() = 0;
};
```

Implementation:

```cpp
class PjsipEngine final : public SipEngine {
    // ...
};
```

Như vậy bạn có thể nói về:

```text
abstraction
dependency inversion
testability
```

### Acceptance criteria

CLI phải làm được:

```text
REGISTER
outgoing call
incoming call
answer
hangup
```

Git tag:

```text
v0.6-pjsip-cli
```

---

# 9. Phase 6 — RTP + Audio

**Trạng thái:** đã nghiệm thu cho lab nội bộ. Source, automated tests, kiến trúc và bằng chứng kiểm thử thực tế nằm trong `docs/phase6/README.md`, `docs/architecture/audio-rtp.md` và `evidence/phase6/`. Milestone release identifier: `v0.7-rtp-audio` (chỉ tạo và push annotated tag sau khi closure commit được xác minh; kiểm tra Git refs để xác nhận trạng thái thực tế).

Thời gian:

```text
5–8 ngày
```

Bây giờ:

```text
ThinkPad + JBL
      │
      │ RTP audio
      ▼
Redmi
```

Bạn test cả:

```text
ThinkPad → Redmi
Redmi → ThinkPad
```

Thêm (ví dụ cấu trúc minh họa; API thực tế xem `src/network/RtpStatistics.*`):

```cpp
struct RtpStatistics {
    std::uint64_t packets_received;
    std::uint64_t packets_lost;
    double packet_loss_percent;
    double jitter_ms;
};
```

Diagnostics (ví dụ định dạng, **không phải số đo** của lần test 75 giây):

```text
[MEDIA] codec=PCMU
[RTP] received=4210
[RTP] lost=14
[RTP] loss=0.33%
[RTP] jitter=6.1ms
```

Bạn cũng phải hiểu:

```text
RTP sequence number
RTP timestamp
SSRC
payload type
RTCP
jitter buffer
codec
```

### Evidence và phạm vi nghiệm thu thực tế

Bằng chứng đã được commit theo quy ước bảo mật hiện tại:

```text
evidence/phase6/automated-verification.txt
evidence/phase6/live-registration.txt
evidence/phase6/live-outgoing-call.txt
evidence/phase6/live-incoming-call.txt
evidence/phase6/phase6-summary.txt
evidence/phase6/rtp-wireshark.txt
```

Các cuộc gọi thực tế hai chiều, codec PCMU 8 kHz và RTP RX/TX đã được xác minh
trong lab. `docs/architecture/audio-rtp.md` giải thích vòng đời media,
conference bridge và cách đọc thống kê; `docs/interview/phase6-questions.md`
phục vụ rà soát kiến thức.

**Phân tích Wireshark đã thực hiện:** một cuộc gọi đi được kiểm soát, kéo
dài khoảng 75 giây, với operator xác nhận nghe được âm thanh hai chiều và
chủ động im lặng khoảng giây 60–75. Hai luồng RTP LAN ngược chiều được
phân tích bằng Wireshark: PCMU 8 kHz, RTP timestamp tăng thông thường
160 đơn vị / 20 ms, 3.698 và 2.960 gói trong các bảng phân tích đã chọn;
cả hai báo cáo 0% packet loss và 0 sequence errors. Các khoảng ngắt có
marker gần cuối ở một luồng được ghi nhận riêng; số liệu không chứng minh
nguyên nhân của việc tạm dừng truyền hay đại diện cho mọi nhánh cuộc gọi.

Tài liệu giao thức `docs/protocols/rtp.md` giải thích RTP header và các
quan sát từ chính capture này; báo cáo đã biên tập nằm tại
`evidence/phase6/rtp-wireshark.txt`. **Chỉ giữ kết quả kiểm thử 75 giây**
trong bằng chứng Wireshark của Phase 6. File `.pcapng` gốc chứa payload
âm thanh chỉ lưu riêng tư ngoài Git; không commit capture, ảnh chưa biên
tập, SIP credentials, Digest headers hoặc keying material.

Git tag cho milestone đã nghiệm thu:

```text
v0.7-rtp-audio
```

---

# 10. Phase 7 — Qt/QML GUI

**Trạng thái:** đã nghiệm thu cho lab nội bộ. Qt/QML build, ViewModel/controller/backend boundary, registration, outgoing/incoming call, clear two-way audio, call duration, diagnostics, automated verification và evidence được ghi tại `docs/phase7/README.md`, `docs/architecture/qt-qml-gui.md` và `evidence/phase7/`. Milestone release identifier: `v0.8-qt-gui` (chỉ tạo/push annotated tag sau khi branch Phase 7 được review, merge vào `main` và trạng thái merged được xác minh).

Thời gian:

```text
5–8 ngày
```

UI đã được triển khai theo architecture `QML -> ViewModel -> CallManager -> SipEngine`. Mock dưới đây là định hướng ban đầu; bản nghiệm thu giữ registration/dialer/incoming/active-call/diagnostics và chưa đưa Mic/Mute thành control Phase 7.

UI:

```text
┌────────────────────────────┐
│ Linux VoIP                 │
│                            │
│ ● Registered               │
│                            │
│ Destination                │
│ [ 1002                  ] │
│                            │
│  1     2     3             │
│  4     5     6             │
│  7     8     9             │
│        0                    │
│                            │
│       [ Call ]             │
└────────────────────────────┘
```

Khi call:

```text
┌────────────────────────────┐
│ 1002                       │
│                            │
│       Connected            │
│                            │
│       00:01:42             │
│                            │
│ Mic     ███████░░          │
│                            │
│ [Mute]         [Hang up]   │
└────────────────────────────┘
```

Dùng structure:

```text
QML
 │
 ▼
ViewModel
 │
 ▼
CallManager
 │
 ▼
SipEngine
```

Không gọi PJSIP trực tiếp từ QML.

Đó là điểm architecture rất đáng nói khi phỏng vấn.

### Evidence

```text
evidence/phase7/
├── README.md
├── automated-verification.txt
├── manual-verification.txt
├── runtime-media.txt
├── login.png
├── dialer.png
├── incoming-call.png
├── active-call.png
└── diagnostics.png
```

Git tag:

```text
v0.8-qt-gui
```

---

# 11. Phase 8 — Multithreading

Thời gian:

```text
5–8 ngày
```

Thread model đã triển khai:

```text
Qt UI thread
    -> CallViewModel
    -> CallManager
    -> ThreadedSipEngine
    -> WorkerExecutor
    -> dedicated SIP worker
    -> concrete SipEngine / Pjsua2SipEngine
```

Phase 8 triển khai:

```text
WorkerExecutor
ThreadedSipEngine
ThreadSafeQueue
mutex
condition_variable
atomic stress-test accounting
dedicated SIP worker
```

Các metrics/logging worker riêng trong sketch ban đầu không được thêm ở Phase 8 vì chưa có yêu cầu runtime cần chúng.

Ví dụ:

```cpp
template<typename T>
class ThreadSafeQueue {
public:
    bool push(T value);
    std::optional<T> wait_and_pop();
    void close();

private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool closed_{false};
};
```

Sau đó stress test.

```text
100,000 events
multiple producers
multiple consumers
```

Chạy ThreadSanitizer nếu khả thi:

```bash
-fsanitize=thread
```

### Evidence

```text
docs/architecture/threading-model.md
```

và:

```text
evidence/phase8/
├── README.md
├── automated-verification.txt
├── stress-test.txt
└── tsan-report.txt
```

Điều nhà tuyển dụng muốn nghe không phải:

> Em biết mutex.

Mà là:

> Em từng gặp shared state giữa SIP callback và UI. Em giải quyết bằng message queue và chỉ update Qt object từ UI thread.

Đó mới là kinh nghiệm có giá trị.

---

# 12. Phase 9 — Debugging, memory và core dump

**Trạng thái:** đã nghiệm thu cho lab nội bộ. Phase 9 giữ nguyên production/threading boundary của Phase 8, bổ sung crash generator cô lập, saved-core GDB analysis, sanitizer-driven lifetime debugging và repaired-path regression. Release identifier: `v0.9-diagnostics` (chỉ tạo/push annotated tag sau khi closure commit được xác minh và merge theo release workflow).

Thời gian:

```text
4–6 ngày
```

Đây là phase portfolio cực mạnh.

Tạo:

```text
tools/crash_generator
```

chứa **bug có chủ đích chỉ dùng trong lab**, ví dụ null dereference.

Chạy:

```bash
ulimit -c unlimited
```

Sau đó phân tích bằng GDB.

Bạn phải có report kiểu:

```text
Crash Analysis

Symptom:
Application terminated with SIGSEGV.

Backtrace:
#0 AudioSession::stop()
#1 CallManager::hangup()
#2 main()

Root cause:
AudioSession had already been destroyed.

Fix:
Changed ownership from raw pointer to std::unique_ptr
and defined explicit object lifetime.

Verification:
Unit test added.
ASan clean after fix.
```

Tạo:

```text
docs/phase9/core-dump-analysis.md
```

Sau đó tạo một lỗi memory riêng:

```text
use-after-free
```

phát hiện bằng:

```text
AddressSanitizer
```

Không giữ bug đó trong production path; dùng lab hoặc commit lịch sử.

### Evidence

```text
evidence/phase9/
├── README.md
├── automated-verification.txt
├── gdb-core-analysis.txt
└── asan-report.txt
```

Một recruiter C++ nhìn phần này sẽ biết ngay bạn đã động tới debugging thực tế.

Git tag:

```text
v0.9-diagnostics
```

---

# 13. Phase 10 — RTSP mini-lab

Softphone chủ yếu cần:

```text
SIP
SDP
RTP
```

nhưng JD trong ảnh còn có **RTSP**.

Đừng cố nhét RTSP vào softphone một cách gượng ép.

Tạo:

```text
labs/rtsp/rtsp-probe
```

Mục tiêu:

```text
RTSP OPTIONS
RTSP DESCRIBE
SDP
RTSP SETUP
RTSP PLAY
```

Bạn có thể chạy local RTSP server rồi dùng probe/client để quan sát protocol.

Viết:

```text
docs/protocols/rtsp.md
```

Trong đó giải thích rõ:

```text
SIP ≠ RTSP

SIP:
session signaling cho communication

RTSP:
control media streaming session

RTP:
media transport
```

Đây là câu interviewer rất dễ hỏi.

Git tag:

```text
v0.10-rtsp-lab
```

---

# 14. Phase 11 — Final Engineering Release

Release:

```text
v1.0.0
```

Feature checklist:

```text
SIP account registration
outgoing call
incoming call
answer
hangup
two-way RTP audio
audio device selection
call state machine
network statistics
Qt/QML GUI
multithreading
IPC monitor
structured logging
unit tests
integration tests
sanitizer build
core dump documentation
```

Architecture cuối:

```text
                    ┌──────────────────────┐
                    │       Qt / QML       │
                    │                      │
                    │ Dialer / Call / Diag │
                    └──────────┬───────────┘
                               │
                         signal / slot
                               │
                    ┌──────────▼───────────┐
                    │      ViewModels      │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │     CallManager      │
                    │     StateMachine     │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │      SipEngine       │
                    │      interface       │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │     PjsipEngine      │
                    └─────┬─────────┬──────┘
                          │         │
                         SIP       RTP
                          │         │
                          ▼         ▼
                       Asterisk   Redmi
```

---

# 15. Bộ bằng chứng quan trọng nhất

Nếu tôi là recruiter, tôi muốn repo của bạn chứng minh được theo bảng này:

| Kiến thức | Bằng chứng trong project |
|---|---|
| C++ | Production code + unit tests |
| OOP | `SipEngine` interface, managers |
| STL | queue, vector, map, optional |
| RAII | socket/file descriptor wrappers |
| Smart pointer | lifetime management |
| Linux | signals, processes, fd, IPC |
| TCP | TCP lab |
| UDP | UDP simulator |
| SIP | `.pcapng` + SIP documentation |
| SDP | actual INVITE/200 OK analysis |
| RTP | RTP capture + statistics |
| RTSP | RTSP probe lab |
| Qt | Qt/C++ integration |
| QML | working GUI |
| Thread | thread model + stress test |
| mutex | ThreadSafeQueue |
| condition_variable | worker/event queue |
| IPC | Unix Domain Socket |
| Design Pattern | State, Observer/interface abstraction |
| Memory debugging | ASan report |
| Core dump | GDB crash analysis |
| GDB | saved backtrace |
| Wireshark | packet captures |
| CMake | multi-target build |
| Git | meaningful commit history/tags |
| Testing | GoogleTest + integration tests |
| Clean Code | clang-tidy + architecture separation |

---

# 16. Đừng chỉ chụp screenshot

Evidence tốt có 4 cấp độ:

```text
Source code
    ↓
Automated test
    ↓
Runtime evidence
    ↓
Written explanation
```

Ví dụ SIP:

```text
source:
PjsipEngine.cpp

test:
test_sip_registration.cpp

runtime:
sip-register.pcapng

explanation:
docs/protocols/sip.md
```

Đây là bằng chứng cực kỳ thuyết phục.

---

# 17. Commit history cũng là bằng chứng

Đừng làm kiểu:

```text
commit 1: init
commit 2: final project
```

Hãy để lịch sử phát triển tự nhiên:

```text
feat(network): implement UDP receiver

test(network): add packet loss tests

feat(sip): add PJSIP account registration

fix(sip): handle registration timeout

feat(call): implement call state machine

fix(threading): remove race in call state callback

test(threading): add queue stress test

feat(ui): add active call screen

fix(audio): handle device disconnect

docs(debug): document core dump investigation
```

Một recruiter mở Git history sẽ thấy quá trình engineering thực tế.

---

# 18. Dùng Git tag cho từng milestone

Tôi khuyên:

```text
v0.1-project-skeleton
v0.2-cpp-core
v0.3-linux-system
v0.4-networking
v0.5-sip-asterisk
v0.6-pjsip-cli
v0.7-rtp-audio
v0.8-qt-gui
v0.9-diagnostics
v0.10-rtsp-lab
v1.0.0
```

Điều này giúp bạn có thể nói:

> Repository thể hiện từng giai đoạn từ raw UDP networking tới SIP softphone hoàn chỉnh.

---

# 19. README cuối cùng nên có cấu trúc này

Ngay đầu:

```text
# Linux VoIP Softphone

A Linux desktop SIP softphone developed in C++20,
Qt/QML and PJSIP.

Features:
- SIP registration
- Incoming/outgoing calls
- RTP audio
- Audio device management
- Network statistics
- Multithreaded architecture
- IPC diagnostics
```

Sau đó:

```text
Demo
Architecture
Features
Technology Stack
Build
Usage
SIP Call Flow
Threading Model
Testing
Debugging
Packet Captures
Known Limitations
Engineering Decisions
Roadmap
```

Đừng viết README dài như sách giáo khoa. Documentation chi tiết nằm trong:

```text
docs/
```

---

# 20. Tạo ADR để chứng minh khả năng System Design

ADR = Architecture Decision Record.

Ví dụ:

```text
docs/adr/ADR-003-state-machine.md
```

Nội dung:

```text
Context

Call lifecycle has multiple asynchronous states.

Options considered

1. if/else based logic
2. state machine
3. State design pattern

Decision

Use explicit state machine.

Reason

Makes invalid transitions detectable and unit-testable.

Consequences

More boilerplate, but easier debugging and testing.
```

Đây là cách rất tốt để chứng minh bạn không chỉ code theo cảm tính.

---

# 21. Demo cuối để đưa cho recruiter

Video khoảng **3–5 phút** là đủ.

Flow:

```text
00:00
show architecture

00:20
launch Asterisk

00:30
launch C++ Qt softphone

00:45
SIP registration

01:00
call Redmi 1002

01:30
two-way audio

02:00
show RTP statistics

02:30
Wireshark SIP flow

03:00
unit tests

03:20
GDB/core dump report

03:45
show GitHub repository
```

Không cần video 20 phút.

---

# 22. Dashboard Diagnostics nên làm

Đây là feature khá hay để project trông thực tế.

```text
Diagnostics

SIP
─────────────────────────
Account        1001
Server         192.168.1.10
Status         Registered

Call
─────────────────────────
Peer           1002
State          Connected
Duration       00:03:42

RTP
─────────────────────────
Codec          PCMU
Packets RX     12,420
Packets Lost   14
Packet Loss    0.11 %
Jitter         5.4 ms

Audio
─────────────────────────
Input          JBL TUNE115TWS
Output         JBL TUNE115TWS
```

Nó vừa đẹp khi demo, vừa cho thấy bạn hiểu system internals.

---

# 23. Definition of Done cho mỗi phase

Tôi khuyên áp dụng quy tắc:

```text
Code works
+
Test exists
+
Evidence exists
+
Documentation exists
=
Phase complete
```

Ví dụ Phase UDP chưa được coi là xong nếu chỉ:

```text
sender chạy
receiver chạy
```

Nó chỉ hoàn thành khi có:

```text
source code
unit test
packet statistics
Wireshark capture
README explanation
Git tag
```

---

# 24. Roadmap timeline thực tế

Nếu mỗi ngày bạn có khoảng 2–3 giờ:

| Tuần | Công việc |
|---|---|
| 1 | CMake + Modern C++ |
| 2 | Linux + threading |
| 3 | TCP/UDP networking |
| 4 | Asterisk + SIP analysis |
| 5 | PJSIP CLI |
| 6 | RTP + audio |
| 7 | Qt/QML |
| 8 | multithreading + IPC |
| 9 | GDB + sanitizer + core dump |
| 10 | RTSP lab |
| 11 | testing/refactor |
| 12 | documentation + final demo |

Đừng ép đúng 12 tuần. Nếu một phase chưa hiểu thì ở lại phase đó.

---

# 25. Ba milestone quan trọng nhất

Bạn nên đặc biệt tập trung vào ba thời điểm này:

```text
Milestone A

UDP packet simulator
+
Wireshark

→ chứng minh networking fundamentals
```

```text
Milestone B

CLI softphone
+
PJSIP
+
Redmi call successfully

→ chứng minh SIP/RTP
```

```text
Milestone C

Qt/QML app
+
multithreading
+
tests
+
debug report

→ chứng minh software engineering
```

Nếu ba milestone này tốt, project đã rất đáng giá.

---

# 26. Khi phỏng vấn, bạn sẽ có thể nói cụ thể

Thay vì:

> Em có tìm hiểu SIP.

Bạn có thể nói:

> Em dựng Asterisk local, tạo hai SIP endpoint 1001 và 1002, capture REGISTER và INVITE bằng Wireshark, sau đó tích hợp PJSIP vào C++ client. Em có lưu packet capture và document toàn bộ REGISTER/INVITE/ACK/BYE flow trong repository.

Thay vì:

> Em biết multithreading.

Bạn nói:

> SIP callback không chạy trên Qt UI thread nên em không update UI trực tiếp. Em đưa event qua một thread-safe queue, sau đó chuyển về UI qua Qt signal/slot. Em cũng chạy stress test và ThreadSanitizer để kiểm tra race condition.

Thay vì:

> Em biết GDB.

Bạn nói:

> Em tạo một crash test, generate core dump, dùng GDB lấy backtrace, xác định lỗi object lifetime, sửa ownership bằng RAII/smart pointer rồi verify lại bằng AddressSanitizer.

Đó là sự khác biệt giữa **“đã học”** và **“đã làm”**.

---

## Tiến độ hiện tại

Repository đã nghiệm thu **Phase 0 đến Phase 9** cho các môi trường đã ghi lại trong evidence. Phase 9 có release identifier:

```text
v0.9-diagnostics
```

Chuỗi milestone đã nghiệm thu:

```text
Phase 0 — Repository + engineering environment
        ↓
Phase 1 — Modern C++ foundation
        ↓
Phase 2 — Linux system programming + IPC
        ↓
Phase 3 — TCP/UDP networking
        ↓
Phase 4 — SIP + Asterisk lab
        ↓
Phase 5 — PJSIP CLI softphone
        ↓
Phase 6 — RTP + Audio
        ↓
Phase 7 — Qt/QML GUI
        ↓
Phase 8 — Multithreading
        ↓
Phase 9 — Debugging, memory và core dump
```

Bước tiếp theo là **Phase 10 — RTSP mini-lab**. Việc tạo/push annotated tag `v0.9-diagnostics` và merge branch là thao tác release riêng biệt; kiểm tra Git refs thay vì suy đoán từ roadmap.

Mục tiêu không phải hoàn thành softphone càng nhanh càng tốt. Mục tiêu là đến cuối project, **mỗi dòng quan trọng trong JD đều có một artifact trong GitHub để bạn chỉ thẳng cho interviewer xem**.
