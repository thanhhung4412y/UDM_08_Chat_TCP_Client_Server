# Kết quả bản thu hồi tin nhắn

Windows 11, Qt 6.12.0, MinGW 64-bit, Release, BUILD_TESTING=ON.

- Build thành công Server, ChatClient và các target kiểm thử/xem trước.
- Chạy toàn bộ CTest: 10/10 bộ qua, 0 lỗi, 228,54 giây: transfer, offline, history, connection, auth, password, ui, profile, search và recall.
- Sau bổ sung menu ở lịch sử/tìm kiếm và hit testing bằng QTextCursor: build lại, chạy 4/4 bộ liên quan (history/ui/search/recall), 0 lỗi, 13,99 giây.
- recall_tests bản cuối chạy riêng: 8 mục PASS gồm init/cleanup, 0 FAIL, 7.483 ms.

Kiểm tra mới:
- Server từ chối người nhận/khách thu hồi tin của người khác, không tin thông báo message_recalled do Client giả mạo; lấy danh tính từ phiên.
- Metadata lưu bền và mở lại; gọi thu hồi lặp không tạo trạng thái trùng. Phân trang quét tối đa 50 bản ghi, không gửi thông tin tin riêng của người khác.
- Client giữ tombstone cho text/private/reply/forward/image/sticker/video, gỡ body/quote/BLOB/path, mở lại vẫn có trạng thái, không hiện lại khi bản gốc đến chậm.
- Tìm kiếm không trả nội dung đã thu hồi; cửa sổ đang tìm tự hủy phản hồi cũ và cập nhật, video link cũ không còn được cho mở.
- Nhận đúng ID tin ở vị trí chuột; thử 180 tin nhiều dòng qua giới hạn tài liệu/giới hạn số mục để tránh chọn nhầm khi doc bị cắt bớt.
- TLS nhiều Client: tin riêng chỉ cập nhật người gửi/người nhận; thu hồi lúc Bob offline rồi đăng nhập lại được đồng bộ; phòng chung cập nhật mọi người; thu hồi lặp vẫn thành công.
- Truyền video mẫu ISO BMFF hoàn tất rồi thu hồi được metadata, tệp người nhận vẫn còn như thiết kế.
- Đóng metadata DB khi shutdown để có thể sao lưu/khởi động lại và dọn thư mục thử. Không còn cảnh báo giữ tệp của bài recall bản cuối.

Đã kiểm tra ảnh Qt thật ở hai kích thước qua ui_preview; dùng dữ liệu giả, không có tài khoản/ảnh riêng thật. Nguồn ZIP không chứa database/chứng chỉ/private key hay EXE.

Giới hạn: tin trước nâng cấp không có metadata Server nên không thu hồi được. Cần cập nhật Server và mọi Client; Client cũ sẽ không hiểu trạng thái mới. Không xóa tệp đã tải, bản sao/trích dẫn/Forward khác, log hoặc backup. SQLite/WAL không phải xóa an toàn trên đĩa. Khi ghi đĩa Client lỗi, UI che trong phiên nhưng cần sửa lưu trữ/đồng bộ lại để giữ trạng thái qua restart.

Không thử trực tiếp hai máy LAN trong đợt này; bài đúng 1 GiB mặc định bỏ qua và không chạy lại. Bộ transfer hồi quy đã chạy; dữ liệu mẫu video chỉ để kiểm tra protocol/hash, không phải phim phát được. Cảnh báo font offscreen không làm lỗi kiểm thử; font xem trước dùng từ Windows và không kèm ZIP.

Log mới: Logs/recall-ctest.txt, recall-ctest-final.txt, recall-ctest-final-detail.txt, recall-tests-final.txt, recall-build*.txt. Báo cáo SEARCH_RESULTS/PROFILE_RESULTS/... và log cũ thuộc phiên bản trước.
