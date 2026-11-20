# Kết quả bản chỉnh sửa tin nhắn

Windows 11, Qt 6.12.0, MinGW 64-bit, Release, BUILD_TESTING=ON.

- Build thành công Server, ChatClient, 11 target kiểm thử và công cụ xem trước.
- CTest bản cuối: 11/11 bộ qua, 0 lỗi, tổng 176,99 giây. Bao gồm transfer, offline, history, connection, auth, password, ui, profile, search, recall, edit.
- edit_tests bản cuối trong CTest qua (6,95 giây). Bài TLS chạy riêng sau áp dụng lại quyền mạng cũng qua, gồm khởi động lại Server (3 mục PASS gồm init/cleanup, 0 FAIL).

Kiểm tra mới:
- Server từ chối sửa tin của người khác, ID không có metadata, nội dung trống/quá dài, ảnh/sticker/video và tin đã thu hồi.
- Base revision cũ bị từ chối, revision tăng và lưu bền qua restart; Server giữ snapshot mới nhất. Thu hồi xóa snapshot sửa trong transaction và không cho sửa để hiện lại.
- Giới hạn 50 mục và 200 KiB dữ liệu JSON/trang: thử 30 nội dung 4.000 ký tự điều khiển cần escape, các khung vẫn dưới MaxFrame và không mất kết quả khi chuyển trang.
- Client giữ sequence/routing/quote, cập nhật body và revision, bỏ bản sửa đến muộn, áp dụng edit trước bản gốc, duplicate bản gốc không ghi đè body mới, thu hồi thắng cả edit đến muộn.
- Mở lại database vẫn tìm được body mới, không trả body cũ (ngoại trừ bản sao/trích dẫn khác như thiết kế).
- Cửa sổ tìm kiếm đang mở được hủy/tải lại khi sửa, body HTML vẫn là chữ, nhãn chỉnh sửa hiển thị, form không gửi nội dung trống/quá dài/không đổi.
- TLS Alice/Bob/Carol: Bob không sửa được tin Alice, tin riêng không lộ cho Carol, Bob offline rồi đăng nhập nhận latest, restart Server cùng dữ liệu/port vẫn đồng bộ, thu hồi sau edit vẫn đúng, tin chung cập nhật mọi người.
- Kiểm tra trực quan hộp chỉnh sửa và chat có nhãn ở hai kích thước cửa sổ qua ảnh Qt thật; toàn bộ dữ liệu minh họa là giả.

Log mới: Logs/edit-ctest.txt, edit-ctest-detail.txt, edit-network-retry.txt, edit-build*.txt. Lần thử TLS ban đầu bị môi trường từ chối loopback với Permission denied; sau áp dụng lại quyền mạng, bài riêng và toàn bộ CTest đều qua. Đây không còn là hạn chế của kết quả cuối.

Các báo cáo RECALL_RESULTS/SEARCH_RESULTS/... và log cũ thuộc bản trước. Cảnh báo font offscreen không làm lỗi kiểm thử. Bài khởi động lại Server có thông báo remote host closed dự kiến vì Server chủ động đóng socket cũ.

Giới hạn: chưa thử trực tiếp hai máy LAN; bài đúng 1 GiB mặc định bỏ qua và không chạy lại. Server/Client mới đều cần cập nhật. Chỉ sửa được tin có metadata xác minh trên Server (từ bản thu hồi trở đi), không sửa bản media hoặc tin đã thu hồi. Không cập nhật các bản sao/Forward/câu trích dẫn riêng khác, log hoặc backup; không có UI xem lịch sử mọi phiên bản. Khi ghi đĩa lỗi, nội dung có thể chỉ cập nhật trong bộ nhớ và tìm kiếm chưa đầy đủ; cần sửa lưu trữ và kết nối lại để đồng bộ.

Thay đổi dữ liệu: Server bổ sung bảng edits trong recalls.sqlite, lưu body mới nhất đã sửa; Client bổ sung message_edits. Không xóa tài khoản, đổi UUID/tên ứng dụng/đường dẫn lịch sử. SQL dùng bind parameters, thay snapshot/thu hồi trong transaction; revision và tombstone chống ghi đè bởi dữ liệu cũ. ZIP không chứa database/private key/EXE hoặc dữ liệu thật.
