# ChatTCP1 — Kiểm thử nâng cấp đăng nhập và nhóm riêng

> Đây là checklist kiểm thử thủ công.

1. Mở Code/CMakeLists.txt bằng Qt Creator Qt 6.12 MinGW và build `chattcp1`, `ChatClient`.
2. Khởi động Server đã lưu cấu hình Gmail trước đó. Mở 3 Client với 3 tài khoản A, B, C.
3. Trên A: chọn **+ Tạo nhóm**; nhập tên `Đồ án`; chỉ chọn B; xác nhận.
4. Xác nhận nhóm hiện ở A và B, **không hiện ở C**.
5. A gửi `Chào B` trong nhóm; B phải nhận; C không được nhận.
6. B trả lời `Chào A` trong nhóm; A nhận; C không nhận.
7. A nhắn riêng cho C bằng cách bấm C ở danh sách trực tuyến; C nhận; B không nhận tin riêng.
8. Chọn nhóm, bấm **+ → Gửi ảnh** và chọn một ảnh. Kiểm tra ảnh hiện ở A và B, C không nhận được ảnh.
9. Đăng xuất B. Từ A, gửi `Tin khi B offline 😃` và một ảnh khác vào nhóm. Khởi động lại Server, đăng nhập B rồi mở nhóm `Đồ án`.
10. Kiểm tra B thấy tin và ảnh gửi lúc offline theo đúng thứ tự. Chuyển qua chat riêng rồi mở lại nhóm để kiểm tra tin và ảnh không bị nhân đôi.
11. Đăng ký/đăng nhập/quên mật khẩu Gmail, gửi ảnh/video giữa hai tài khoản, SQLite lịch sử tin riêng và cập nhật avatar phải được kiểm thử hồi quy.
12. Kiểm tra UI không còn `Phòng chung` và không thể gửi gói `chat` công khai qua Server.
13. Khi A, B và C online, A chọn nhóm `Đồ án` rồi bấm **+ → Gửi video MP4**. Kiểm tra A và B thấy tiến độ, B mở được tệp khi hoàn tất, C không nhận video. Chuyển sang chat riêng rồi mở lại nhóm trong cùng phiên để kiểm tra liên kết video còn dùng được.
14. Đăng xuất B rồi thử gửi video vào nhóm từ A. Server phải báo không có thành viên khác online. B đăng nhập lại và xác nhận video đó không được gửi bù.

Chỉ thành viên được Server xác nhận mới có quyền gửi và tải lịch sử nhóm. Người được mời phải đang online khi tạo nhóm. Mỗi nhóm có tối đa 16 thành viên, kể cả người tạo.
