# Kết quả bản tài khoản/TLS — 30/10/2026

## Đã xác nhận

- Windows 11, Qt 6.12.0 MinGW 64-bit, GCC 13.1, CMake/Ninja, Release.
- Client, Server và tất cả thư viện/bộ kiểm thử build thành công.
- CTest: **5/5 bộ qua, 0 bộ lỗi**, 95,18 giây: transfer_tests, offline_tests, history_tests, connection_tests, auth_tests.
- Bài bổ sung `registrationConfirmation` chạy riêng sau đó: PASS. Nhập lại mật khẩu không khớp không được chấp nhận; khớp thì tạo cấu hình đăng ký; hộp mới trở về đăng nhập và mật khẩu trống.
- Bài riêng **1 GiB sau xác thực qua TLS**: PASS, **78,684 giây** cho lượt chạy đó. Nguồn/đích đúng **1.073.741.824 byte**, SHA-256 khớp; SQLite hai bên có đúng một mục video hoàn tất và trỏ đúng tệp nhận.

Bài 1 GiB tùy chọn được bật riêng bằng CHAT_RUN_1GIB=1. Trong CTest mặc định các bài 1 GiB không chạy; kết quả trên lấy từ lượt chạy riêng, không tính nhầm SKIP thành PASS.

## Kiểm thử tài khoản và danh tính Server

- Tạo tài khoản, đóng/mở lại SQLite rồi đăng nhập: UUID tài khoản không đổi.
- Tên khác hoa/thường là cùng tài khoản. Đăng ký trùng bị từ chối; mật khẩu sai bị từ chối.
- Hai tài khoản cùng mật khẩu có salt và hash khác nhau; salt 16 byte, hash 32 byte, 600.000 vòng và algorithm lưu đúng.
- QSettings không lưu mật khẩu; tải cấu hình/hộp nhập mới có mật khẩu trống, ô mật khẩu dùng chế độ che ký tự.
- Tạo/nạp lại danh tính TLS Windows: chứng chỉ và private key hợp lệ, fingerprint không đổi sau nạp lại.
- Kết nối đăng ký/đăng nhập bằng QSslSocket thực: socket mã hóa, tài khoản xác minh thành công.
- Từ chối tin cậy Server thì không đi tới xác thực; JsonChannel không gửi application JSON trên QSslSocket chỉ mới kết nối TCP nhưng chưa mã hóa.
- Gói join chỉ có tên và gói chat trước xác thực không được chấp nhận.
- Đổi chứng chỉ tại cùng endpoint phát hiện fingerprint khác và yêu cầu quyết định mới; từ chối thì không đăng nhập.
- Hai yêu cầu đăng ký đồng thời cùng tên: đúng một thành công.
- Yêu cầu thứ 11/IP trong cửa sổ giới hạn bị từ chối; trong lúc PBKDF2 chạy, timer vòng lặp giao diện vẫn hoạt động.
- Đăng nhập lại sau lỗi/mất kết nối; tài khoản đang online không vào phiên thứ hai; sau đóng phiên cũ, thử lại được.

## Giữ chức năng hiện có

Các bộ hồi quy đã chuyển sang dùng Server TLS và tài khoản trong thư mục tạm:

- Chat chung/riêng, Reply/Forward, sticker, JPEG, UUID/thời gian của tin nhắn.
- Video gửi riêng/phòng chung, nhiều người gửi, người nhận chậm, ACK, hủy, mất kết nối, timeout và dữ liệu lỗi.
- SQLite lưu/đọc lại đủ loại tin, ảnh BLOB, đường dẫn video, phân trang, tách tài khoản/endpoint, lỗi database và ảnh xem trước có giới hạn.
- Luồng nhận tin qua TLS/Server/tài khoản rồi lưu SQLite và đọc lại sau mở database.
- Kiểm tra khung JSON tách/gộp vẫn có bài raw TCP độc lập cho lớp framing; đó không phải chế độ đăng nhập TCP thuần của ứng dụng.

## Phạm vi và giới hạn

- Thử qua loopback thật trên một máy, chưa thử hai máy vật lý/router/tường lửa của bạn.
- Tệp 1 GiB là dữ liệu ISO BMFF tổng hợp để so byte/hash, không phải phim có thể phát. Nên thử thêm một MP4 thực sau khi đăng nhập và mở lại từ lịch sử.
- Chưa kiểm toán bảo mật độc lập, thử tải Internet, mất điện/ổ đĩa hỏng hoặc cài bộ EXE trên máy sạch không có Qt.
- Chưa tự động hóa toàn bộ thao tác giao diện native của main window, xác nhận fingerprint bằng mắt và thao tác đăng xuất giữa một video đang truyền; các phần xử lý tương ứng được kiểm thử qua module và nên thử bằng tay khi demo.
- Không có khôi phục mật khẩu, đổi mật khẩu/tên, quản trị hoặc mã hóa tệp lịch sử trên đĩa trong phạm vi bản này.
- Phông chữ offscreen trong bộ widget test có thể cảnh báo thiếu thư mục phông của Qt; các kiểm tra cấu hình/văn bản/ảnh đều qua.

## Log

Các file **accounts-*** trong Logs/ thuộc bản này. File có tiền tố lan- và các báo cáo LAN_RESULTS.md/SQLITE_RESULTS.md là bằng chứng của bản trước, được giữ riêng. Database, password người dùng, certificate/private key thực và build artifact không được đưa vào ZIP.

## Kiểm tra nhanh bằng tay

1. Đóng Server/Client cũ. Mở Code/CMakeLists.txt, Configure/Build và chạy Server mới.
2. Chọn cổng, mở Client, chọn Dang ky tai khoan moi, nhập tên/mật khẩu/xác nhận và IP/cổng.
3. Đối chiếu SHA-256 với Server thật. Chọn No trước để kiểm tra từ chối, rồi thử lại và chọn Yes khi đã khớp.
4. Đăng ký/đăng nhập đúng rồi thử chat/ảnh/sticker/Reply/Forward/video với tài khoản thứ hai.
5. Đăng xuất, thử sai mật khẩu: không được vào, không tải lịch sử cũ trên cửa sổ mới. Đăng nhập đúng: lịch sử được khôi phục.
6. Thử đăng nhập cùng tài khoản ở hai Client; đóng phiên cũ và thử lại.
7. Đóng/mở lại cả Server và Client: tài khoản vẫn dùng được, fingerprint vẫn như cũ, Client vẫn không điền lại mật khẩu.
