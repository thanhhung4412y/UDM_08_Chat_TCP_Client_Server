# Kết quả bản trạng thái tin riêng

Windows 11, Qt 6.12.0, MinGW 64-bit, Release, BUILD_TESTING=ON.

- Build thành công Server, ChatClient, 12 target kiểm thử và các công cụ xem trước.
- Chạy toàn bộ CTest: 12/12 bộ qua, 0 lỗi, tổng 188,60 giây (transfer, offline, history, connection, auth, password, ui, profile, search, recall, edit, receipt).
- Sau hoàn thiện cache và xử lý lỗi ghi trạng thái: build lại, chạy 6/6 bộ liên quan history/ui/search/recall/edit/receipt, 0 lỗi, 27,95 giây.
- receipt_tests có thêm bài database bị khóa ở bản cuối. Lần chạy riêng trước bổ sung lỗi ghi: 7 mục PASS gồm init/cleanup, 0 FAIL, 8.667 ms; bản cuối qua toàn bộ trong CTest.

Kiểm tra mới:
- Sent được tạo từ metadata Server; chỉ recipient đã xác thực được ACK 2/3. Sender và người thứ ba không tự đánh dấu xem thay recipient, không nhận trạng thái tin riêng của người khác.
- State/revision chỉ tăng, duplicate/stale không làm giảm trạng thái. Batch sai rollback toàn bộ thay đổi; ID không theo dõi/không có quyền bị bỏ qua, không lộ dữ liệu thêm.
- Lưu bền ở Server/Client, pending receipt trước bản gốc được áp dụng sau, giữ nguyên body/sequence/quote, không hiện trạng thái trên tombstone.
- Inventory quét 200 metadata/lượt, ACK tối đa 50 tin; thử 205 own outgoing xen với 123 incoming để kiểm tra cursor không lặp/mất và không đọc BLOB ảnh. Username tiếng Việt/case được kiểm tra ở phía Client.
- Nhãn chỉ trên own private: Sent/Received/Seen; incoming và phòng chung không hiển thị. Hidden/minimized/modal không đạt điều kiện seen.
- TLS Alice/Bob/Carol: Server echo state Sent, Bob chưa mở đúng peer chỉ Received, mở nhầm Carol không Seen, read guard false không Seen, đúng Alice và guard true mới Seen.
- Alice offline khi Bob xem rồi đăng nhập lại được đồng bộ; restart Server cùng data/port vẫn giữ state; sửa nội dung giữ trạng thái, thu hồi bỏ nhãn và body.
- Ảnh, sticker, video riêng hoàn tất được Received/Seen. Video dùng dữ liệu ISO BMFF mẫu để kiểm tra protocol, không phải phim phát thật.
- Database bị giữ write lock: UI/cache giữ Seen khi receipt cũ đến muộn; sau mở khóa, receipt Sent cũ không xóa cache Seen, đồng bộ Seen mới lưu được và restart Client vẫn đúng. Client dừng ACK lặp và defer cập nhật bộ nhớ sau lỗi ghi.
- Kiểm tra ảnh Qt thật ở 1120×760 và 800×620 với ba nhãn; dữ liệu minh họa giả, không truy cập tài khoản thật.

Log mới: Logs/receipts-ctest.txt, receipts-ctest-final.txt, receipts-ctest-final-detail.txt, receipt-tests.txt, receipts-build*.txt. Các báo cáo EDIT_RESULTS/RECALL_RESULTS/... và log cũ là kết quả phiên bản trước.

Thay đổi dữ liệu: receipts trong recalls.sqlite; message_receipts và flag không theo dõi trong SQLite lịch sử Client. Không đổi tài khoản/UUID/đường dẫn dữ liệu. Actor lấy từ phiên Server, đối chiếu recipient; client chỉ gửi ID/state. Thông báo chỉ tới sender/recipient. Video chunk/hash/ACK file giữ nguyên; trạng thái áp dụng sau lưu hoàn tất.

Giới hạn: chưa thử tay trên hai máy LAN. Foreground/peer được mô phỏng trong test Controller; helper Qt được kiểm tra điều kiện hidden/minimized/modal, không tuyên bố đã xác minh thao tác thật trên desktop của người dùng. Seen là cửa sổ đúng cuộc trò chuyện mở ở phía trước, không phải bằng chứng đọc hiểu/xem hết video. Không có trạng thái phòng chung hoặc hộp thư offline. Client cũ không gửi ACK; metadata trước bản thu hồi có thể không theo dõi được. Khi đĩa lỗi, trạng thái có thể chỉ ở RAM đến lúc sửa lỗi và đồng bộ lại.

Bài đúng 1 GiB mặc định bỏ qua và không chạy lại; bộ transfer hồi quy đã chạy. Cảnh báo font offscreen và remote host closed khi bài thử chủ động restart Server không làm lỗi kiểm thử. ZIP không chứa database, chứng chỉ/private key, EXE hoặc dữ liệu thật; font hệ thống không phân phối kèm.
