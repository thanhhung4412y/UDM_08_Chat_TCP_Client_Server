# Kết quả kiểm thử — 29/10/2026

## Môi trường và build

- Windows 11, Qt 6.12.0 MinGW 64-bit, CMake/Ninja, compiler GCC 13.1.
- Build Release thành công: ChatClient, chattcp1, chat_history, video_transfer, history_tests, history_probe, transfer_tests, offline_tests.
- Đã tìm thấy và sử dụng Qt Sql / QSQLITE đi kèm kit. Không cần cài dịch vụ SQLite.
- Hỗ trợ kết nối nội bộ đã được cấp trong lượt này: các bài TCP dưới đây dùng **socket thật**, không còn bị chặn như lượt video trước.

## Kết quả

| Kiểm tra | Kết quả |
|---|---|
| CTest: transfer_tests, offline_tests, history_tests | 3/3 bộ qua, 0 bộ lỗi; lượt này 44,16 giây |
| history_tests sau lần điều chỉnh ảnh xem trước cuối cùng | 18 PASS, 0 FAIL, 0 SKIP; 4,143 giây |
| sqliteOverTcpAndRestart trên bản build cuối | PASS |
| Video đúng 1 GiB qua TCP + bản ghi SQLite hai bên | PASS; 119,763 giây |

Bài 1 GiB được bật riêng bằng `CHAT_RUN_1GIB=1`. Trong lượt CTest mặc định, các bài dung lượng 1 GiB tùy chọn được bỏ qua; kết quả chạy riêng ở dòng cuối đã xác nhận TCP 1 GiB thực tế.

Lượt 1 GiB xác minh:

- Nguồn và tệp nhận đúng **1.073.741.824 byte**.
- SHA-256 nguồn và đích khớp.
- Hai bên không báo lỗi truyền video.
- SQLite người gửi và người nhận mỗi bên có đúng **một** mục video hoàn tất; mục nhận trỏ đúng tệp thực tế và lưu đúng dung lượng.
- Dữ liệu thử có chữ ký ISO BMFF mẫu và nội dung tổng hợp. Đây là kiểm thử truyền byte, không phải kiểm thử giải mã/phát một bộ phim.

Một lượt 1 GiB đầu tiên dùng cơ chế chờ thăm dò trong Qt Test đã được chủ động dừng; sau đó đổi riêng cách chờ trong bài kiểm thử sang vòng lặp sự kiện gốc và chạy lại thành công như log đính kèm. Mã truyền video của ứng dụng không bị thay đổi để làm bài thử đó qua.

## Phạm vi đã kiểm tra

### SQLite và tải lại

- Lưu/đóng/mở database với chat, tin riêng, Reply, Forward, sticker, JPEG BLOB và metadata video.
- Khởi chạy **hai tiến trình riêng biệt**: tiến trình đầu ghi rồi thoát; tiến trình sau mở lại và đọc đúng Reply tiếng Việt có ký tự đặc biệt.
- Phân trang 125 mục, thứ tự theo sequence; đọc trang cũ không lệch khi có mục mới xen vào.
- Hai kết nối SQLite cùng tệp; UUID trùng không tạo thêm dòng.
- Tách đường dẫn theo tên/Server/cổng; cách viết hoa/thường của cùng tên dùng cùng không gian.
- Tin riêng không thuộc người dùng hiện tại bị bỏ qua; gói chia video và system/users/error không được lưu như tin nhắn.
- Database hỏng hoặc schema mới hơn không bị tự xóa/ghi đè; lỗi thư mục, lỗi khóa database và thiếu kết nối lưu được báo. Với lỗi lưu, tin hợp lệ vẫn hiện trực tiếp.
- HTML trong nội dung/trích dẫn hiển thị dưới dạng văn bản, không được chèn thành thẻ HTML.
- Khôi phục ảnh từ BLOB; ảnh xem trước tối đa 240×240; thay trang giải phóng tài liệu/cached resource cũ.
- Liên kết video chỉ mở các tệp đã đăng ký trong trang đang xem. Tệp bị xóa/di chuyển hoặc liên kết ngoài danh sách được báo lỗi.

### TCP và giữ tính năng cũ

- Gửi chat, tin riêng, Reply, Forward, sticker, JPEG trong lúc video 17 MiB + 37 byte đang truyền.
- Bản tin gửi về người gửi và người nhận có cùng UUID, không làm lộ tin riêng tới Client thứ ba.
- Gửi video phòng chung, hai người gửi đồng thời, người nhận chậm giữ ACK, hủy và mất kết nối.
- Khung bị tách/gộp, khung không newline vượt giới hạn, offset sai, Base64 sai, khối quá lớn, tổng byte quá khai báo, chữ ký MP4 sai, kết thúc thiếu và checksum sai.
- Luồng end-to-end nhận đủ bảy loại tin (chat/private/reply/forward/sticker/image/video) qua TCP, lưu SQLite, đóng/mở lại database và kiểm tra nội dung ảnh/đường dẫn video.
- Các bài video mô phỏng bổ sung xác minh historyReady chỉ phát sau hoàn tất, không phát khi hủy giữa chừng.

## Chưa kiểm tra tự động

- Phát MP4 có codec thật bằng ứng dụng phát mặc định; thao tác người dùng toàn bộ giao diện Qt Creator/Client bản mới.
- Mất điện đột ngột, ổ đĩa hỏng hoặc đầy đúng lúc commit; kiểm thử hiện có dùng lỗi đường dẫn, khóa DB và dữ liệu hỏng để kiểm tra các nhánh báo lỗi.
- Nhiều ngày dữ liệu, ổ mạng/chia sẻ, đóng gói cài đặt trên máy chưa có Qt.
- Đồng bộ lịch sử/offline delivery không thuộc tính năng của bản này.

Có một cảnh báo phông chữ khi chạy widget bằng nền tảng offscreen của Qt trong môi trường kiểm thử; các kiểm tra văn bản, ảnh và liên kết đều qua. Không có thay đổi cài đặt phông chữ hệ thống.

## Kiểm tra nhanh bằng tay

1. Mở Code/CMakeLists.txt, Configure/Build và chạy Server + hai Client mới.
2. Gửi tin, Reply, Forward, ảnh, sticker và một MP4 thật. Đóng Client, mở lại cùng tên.
3. Kiểm tra 100 mục gần nhất, bấm Lich su, dùng Cu hon/Moi hon/Moi nhat.
4. Mở video từ lịch sử. Sau khi di chuyển một video thử ra chỗ khác, xác nhận ứng dụng báo tệp không còn ở đường dẫn cũ.
5. Đăng nhập bằng tên khác: không thấy tin riêng của tài khoản trước. Đường dẫn database hiện trong hộp Lich su.

Log có trong thư mục **Logs/**. Mọi database trong bài thử dùng dữ liệu tổng hợp, không đưa lịch sử người dùng vào ZIP.
