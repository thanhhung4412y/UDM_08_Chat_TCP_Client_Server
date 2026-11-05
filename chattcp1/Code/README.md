# Code

Tất cả mã nguồn C++/Qt nằm tại đây, theo gợi ý `Server/`, `Client/`, `Shared/` trong cấu trúc mẫu.

```text
Code/
├── CMakeLists.txt           # Cấu hình build cả hai ứng dụng
├── Server/                  # main.cpp, chatserver.h/.cpp, mainwindow.h/.cpp/.ui
├── Client/                  # main_client.cpp, stickers.qrc, sticker/*.jpg
└── Shared/                  # Để trống: chưa có lớp dùng chung
```

**Mở `Code/CMakeLists.txt` bằng Qt Creator để chạy dự án.** Các đường dẫn trong CMake đã cập nhật theo vị trí file mới; tài nguyên trong `Client/stickers.qrc` vẫn tìm ảnh ở `Client/sticker/`.

Không đưa `build/`, `build_backup/`, file biên dịch hay cấu hình máy cá nhân lên Git.
