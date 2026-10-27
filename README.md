# LR Module Sampler — Dual Channel Sampler (VST3 + Standalone)

## Cara pakai
| Kontrol | Fungsi |
|---|---|
| **LCD** | Nama preset, lalu baris sampling (nama file sampling menggantikan "Sampling 1/2") dengan meter output + nilai dB |
| **SELECT (CH1/CH2)** | Pilih channel. Knob parameter utama bekerja untuk channel ini. LCD berubah ke FFT input + node HPF/LPF (geser node) + nama sampling |
| **WAVE** | Masukkan sampling ke channel yang di-SELECT. Tanpa SELECT tidak bekerja |
| **Knop PRESET** | Putar untuk memilih preset. **Klik kiri** pada knop untuk me-restore preset yang dipilih |
| **SAVE** | Simpan semua parameter, sampling, dan level fader; muncul popup Rename |
| **RESET** | Hapus semua preset dan sampling (dengan konfirmasi) + parameter kembali default |
| **LINK** | Gabungkan output 1 dan 2 (kedua output membawa jumlah kedua channel) |
| **CANCEL** | Kembali ke tampilan awal LCD |
| **MUTE / FEDER** | Mute dan level output tiap channel |
| **Knop SENSITIV (strip)** | Sama dengan parameter Sensitivity channel tersebut |

### Parameter (per channel)
- **Sensitivity** — penguatan input sebelum deteksi (−12 … +36 dB). Tinggi = pukulan lembut pun berbunyi.
- **HPF / LPF Input** — 20 Hz – 20 kHz, 24 dB/oct, mencegah bocor antar pad.
- **Threshold** — batas minimum agar trigger aktif.
- **Retrigger** — jeda 5–500 ms setelah satu pukulan (cegah bunyi ganda).
- **Velocity** — hubungan kekuatan pukulan ↔ volume + kecerahan suara. Knob di minimum = **OFF** (volume tetap).

### Lokasi data
Preset (`.scpreset`) dan salinan sampling disimpan di folder `SampleCore/` pada direktori application-data pengguna
(Windows `%APPDATA%`, macOS `~/Library`, Linux `~/.config`).

## Catatan teknis
- Jendela GUI 984×404 px (menyesuaikan susunan aset yang ada).
- Deteksi membaca puncak hanya ~0,6 ms setelah threshold terlewati (untuk velocity), jadi tambahan latensi trigger < 1 ms. Latensi total hampir seluruhnya ditentukan buffer audio perangkat.
- **Standalone low latency**: saat pertama dijalankan, standalone otomatis memilih ASIO, atau *Windows Audio (Low Latency Mode)* / Exclusive Mode jika ASIO tidak tersedia, dengan buffer 128 sampel. Judul jendela menampilkan tipe driver, buffer, dan latensi. Menu **Audio** di jendela: *Pengaturan Audio...* dan pilihan cepat buffer 64 / 128 / 256 / 512. Jika bunyi pecah/retak, naikkan buffer satu tingkat. Pilihan tersimpan otomatis.
- **ASIO (Windows)**: aktif di build Windows. ASIO SDK Steinberg tidak disimpan di repo; workflow mengunduhnya otomatis (atau taruh manual di `third_party/asiosdk`, harus ada `common/iasiodrv.h`). Saat pertama dijalankan, standalone memilih tipe **ASIO** lebih dulu (driver Maono diutamakan), lalu Low Latency Mode, lalu Exclusive Mode. Pakai driver ASIO dari audio interface-mu; ASIO4ALL/FL Studio ASIO dilewati kecuali satu-satunya.
- Polifoni 8 voice per channel, sampling dimix ke mono dan dibatasi 30 detik.
