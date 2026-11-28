# Android

arCade berjalan di Android sebagai satu file **.apk**, dibangun tanpa Gradle, tanpa Android Studio, dan tanpa kode Java/Kotlin. Dokumen ini menjelaskan apa yang ada, cara membangunnya sendiri, cara menyiapkan kunci penandatanganan rilis, dan apa yang **belum** terbukti.

## Ringkasan

| | |
|---|---|
| Hasil | `arCade-vX.Y.Z-android-arm64.apk` di halaman Releases (satu file, seperti build Linux dan Windows) |
| Perangkat | Android 8.0 (API 26) ke atas, CPU **arm64-v8a** (hampir semua HP modern; emulator x86 belum didukung) |
| Orientasi | landscape (kedua arah), layar penuh |
| Yang bisa dimainkan | hub + keempat game di menu (`beat_dodge`, `different_color`, `gritty_golf`, `loop_lap`) dengan kontrol sentuh. Game di `deferred/` ikut terbangun tetapi **belum punya kontrol sentuh** (arsip tetap terkunci sampai vault terisi) |
| Leaderboard online | **mati** di build Android (tab Online tidak muncul). Cara menyalakannya nanti: lihat [bawah](#menyalakan-leaderboard-online-nanti) |
| Skor | disimpan di folder privat aplikasi (`scores.txt`); terhapus saat aplikasi di-uninstall |
| Identitas aplikasi | package `io.github.zaxx_q.arcade`. **Jangan diubah** setelah ada yang memasangnya: skor, update, dan kunci terikat pada nama ini (satu baris di `android/AndroidManifest.xml`, dan `PACKAGE` di `tools/verify_apk.sh`) |

## Memasang dari Releases

1. Unduh `arCade-vX.Y.Z-android-arm64.apk` di HP, lalu buka. Android akan meminta izin "pasang aplikasi dari sumber ini" untuk browser atau file manager yang dipakai.
2. Play Protect mungkin memperingatkan bahwa aplikasi tidak dikenal (aplikasinya tidak lewat Play Store). Itu wajar untuk APK dari luar toko.
3. Versi baru dipasang **di atas** versi lama (kunci penandatangan sama, `versionCode` naik), skor tetap ada. `SHA256SUMS.txt` di rilis dipakai untuk memeriksa unduhan.

APK dari menu **Actions -> Android -> Run workflow** (atau dari pull request) memakai kunci sekali pakai: itu hanya untuk mencoba. Android menolak memasangnya di atas rilis (dan sebaliknya), jadi uninstall dulu; skor ikut hilang.

## Kontrol sentuh

Layar sentuh dipakai lewat lapisan yang sama untuk semua game (`core/src/touch.c`, murni C dan diuji; `core/gfx/gfx_touch.c` menggambar dan membaca jari):

- Joystick virtual di kiri bawah (arah, dan menu), tombol aksi bulat di kanan bawah (Enter/Spasi), tombol kecil kembali (Esc) di atas joystick, dan tombol tambahan sesuai game di atas tombol aksi.
- Jari pertama yang menyentuh sebuah kontrol "memilikinya" sampai diangkat; jari kedua bebas dipakai kontrol lain (bergerak sambil menembak).
- Hub: ketuk item menu, ketuk petunjuk di bawah layar (L, N, B...), dan keyboard layar muncul untuk mengisi nama pemain.
- Tombol Back Android diteruskan ke game sebagai `KEY_BACK` (sama dengan Esc).
- Di komputer: `ARCADE_TOUCH_UI=1 build/dev/bin/arCade` memakai tampilan sentuh dengan mouse sebagai satu jari, tanpa perlu HP.

## Cara kerjanya

```
cmake --preset android-arm64  ->  libarcade.so      (NDK, C99, raylib platform Android; tests dan tools/ tidak ikut)
tools/build_apk.sh            ->  arCade-....apk     (aapt2 + zip + zipalign -P 16 + apksigner)
tools/verify_apk.sh           ->  pemeriksaan dengan tool asli (dijalankan CI setelah build)
```

| File | Peran |
|------|-------|
| `android/AndroidManifest.xml` | satu `NativeActivity` (`hasCode="false"`, `android.app.lib_name = arcade`), landscape, tanpa permission, `extractNativeLibs="false"` |
| `android/res/`, `android/make_icon.py` | ikon adaptif dan tema layar penuh; PNG dibuat ulang dengan `python3 android/make_icon.py` (butuh Pillow) |
| `cmake/ArcadeRaylib.cmake` | raylib `PLATFORM=Android`; mencari NDK lewat `ANDROID_NDK`, `CMAKE_ANDROID_NDK`, atau path toolchain |
| `app/CMakeLists.txt` | hub menjadi `libarcade.so` (SHARED), `-u ANativeActivity_onCreate`, alignment 16 KB, di-strip saat Release |
| `core/gfx/gfx_android.c` | folder data privat, suara dibisukan saat aplikasi tidak di depan |
| `tools/build_apk.sh` | membuat, meluruskan (zipalign), dan menandatangani APK; `--version-code X.Y.Z` mencetak `versionCode` |
| `tools/verify_apk.sh` | memeriksa APK jadi: paket, versi, SDK, ABI, tanpa permission, tanda tangan, alignment 16 KB, library hanya butuh library sistem |
| `.github/workflows/android.yml` | workflow yang dipanggil `release.yml` dan `ci.yml`, atau dijalankan manual |

`versionCode = (major * 1000 + minor) * 1000 + patch`: selalu naik mengikuti versi, itu yang dibandingkan Android untuk memutuskan "update". Karena itu `minor` dan `patch` harus di bawah 1000. `minSdk` 26 dan `targetSdk` 35 ada di `tools/build_apk.sh`.

**Kenapa targetSdk 35, bukan 36:** untuk aplikasi yang menargetkan 36, Android 16 menyalakan predictive back secara bawaan sehingga tombol Back tidak lagi sampai ke game sebagai tombol keyboard. `android:enableOnBackInvokedCallback="false"` menahannya, tetapi Google menyebutnya sementara. Naik ke 36 butuh game yang menangani Back dengan cara baru (perlu kode Java/JNI), jadi ditunda.

## Membangun sendiri

Butuh: **NDK r28 atau lebih baru** (r28 meluruskan segmen ke 16 KB sendiri; flag di CMake menjaga NDK lama tetap benar), Android SDK **build-tools 35.0.0+** (zipalign `-P 16`) dengan platform `android-35`, JDK (untuk `keytool` dan `apksigner`), `zip`, Ninja, dan CMake 3.25+.

```bash
export ANDROID_HOME=$HOME/Android/Sdk                     # SDK: build-tools + platforms
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/28.2.13676358   # NDK yang dipakai CI
cmake --preset android-arm64 -DARCADE_VERSION=0.0.0-dev && cmake --build --preset android-arm64
bash tools/build_apk.sh --lib build/android-arm64/lib/libarcade.so --version 0.0.0-dev --out dist/arCade-test.apk
bash tools/verify_apk.sh --apk dist/arCade-test.apk --version 0.0.0-dev   # butuh readelf (binutils) atau READELF=llvm-readelf NDK
adb install -r dist/arCade-test.apk && adb logcat                         # log di HP (cari "raylib" dan "arCade")
```

Tanpa `--keystore`, `build_apk.sh` membuat kunci sekali pakai. Variabel `BUILD_TOOLS_VERSION`, `AAPT2`, `ZIPALIGN`, `APKSIGNER`, dan `ANDROID_JAR` mengganti pencarian tool (lihat `bash tools/build_apk.sh --help`). Tes skrip (tanpa SDK, dengan tool tiruan): `bash tools/tests/test_build_apk.sh && bash tools/tests/test_verify_apk.sh`.

## Kunci penandatangan rilis (sekali saja)

Android hanya mau memperbarui aplikasi jika APK baru ditandatangani **kunci yang sama**. Kunci rilis dibuat sekali, disimpan sebagai secret repository, dan dipakai `release.yml`. Build rilis **gagal dengan sengaja** bila secret-nya belum ada: APK dengan kunci sekali pakai tidak boleh sampai ke rilis (tidak akan bisa diperbarui). Rilis Linux dan Windows tetap terbit tanpa APK (lihat [CI dan rilis](#ci-dan-rilis)).

```bash
keytool -genkeypair -v -keystore arcade-release.jks -alias arcade -keyalg RSA -keysize 2048 -validity 10000
base64 -w0 arcade-release.jks > arcade-release.jks.b64        # macOS: base64 -i arcade-release.jks
gh secret set ARCADE_KEYSTORE_B64 < arcade-release.jks.b64
gh secret set ARCADE_KEYSTORE_PASSWORD                         # password keystore (diminta di terminal)
gh secret set ARCADE_KEY_ALIAS --body arcade                   # opsional: bawaannya "arcade"
# ARCADE_KEY_PASSWORD opsional: kosong berarti sama dengan password keystore (jenis PKCS12 memang menyamakannya)
keytool -list -v -keystore arcade-release.jks | grep "SHA256:"
gh variable set ARCADE_KEY_SHA256 --body "<sidik jari SHA256 di atas>"
```

- `ARCADE_KEY_SHA256` (variable, bukan secret) membuat build rilis gagal bila terpakai kunci yang berbeda dari yang diharapkan, daripada menerbitkan APK yang tidak bisa memperbarui rilis sebelumnya. Sidik jari sertifikat dicetak di ringkasan setiap build rilis.
- **Simpan cadangan `arcade-release.jks` dan passwordnya di tempat aman di luar repository** (pengelola password, bukan git: `*.jks` jangan pernah di-commit). Jika hilang, pengguna harus uninstall dulu sebelum bisa memasang versi baru, dan skor mereka hilang.
- Hanya empat secret itu yang terlihat oleh job Android (`release.yml` menamainya satu per satu; tidak ada `secrets: inherit`), dan hanya pada rilis. Pull request dan run manual tidak menyentuhnya.

## CI dan rilis

- `release.yml` -> job `build-android` -> `android.yml` (`release: true`) -> artifact `release-android` -> ikut dipublikasikan dan masuk `SHA256SUMS.txt`.
- **Belum wajib.** Sampai satu rilis berhasil membawa APK, kegagalan job Android (kunci belum ada, masalah NDK) tidak menahan rilis Linux dan Windows: `publish` jalan tanpa APK dan memberi peringatan di ringkasan. Setelah terbukti hijau, hapus baris `if:` di job `publish` pada `release.yml` supaya Android wajib (lihat [RELEASING.md](RELEASING.md)).
- `ci.yml` menjalankan job `android` pada pull request yang menyentuh kode C, CMake, atau paket Android. Job ini juga belum termasuk `CI OK`; tambahkan ke `needs` `ci-ok` setelah hijau.
- `android.yml` mem-pin NDK `28.2.13676358` (r28c), build-tools `35.0.0`, dan platform `android-35`. Kalau image runner tidak membawanya, `sdkmanager` memasangnya.

## Yang belum terbukti

APK ini ditulis di lingkungan yang tidak bisa mengunduh NDK maupun Android SDK, jadi **belum pernah dikompilasi dengan NDK asli dan belum pernah dijalankan di HP atau emulator**. Yang sudah diperiksa: logika sentuh (tes unit), tampilan sentuh di smoke test headless dengan jari skrip, konfigurasi CMake terhadap NDK tiruan, sintaks sumber Android dengan `-Werror`, dan seluruh skrip paket dengan tool tiruan. Yang hanya bisa dibuktikan oleh run CI pertama dan sebuah HP:

1. `Actions -> Android -> Run workflow`: apakah NDK mengompilasi dan menautkan `libarcade.so`, apakah `tools/verify_apk.sh` lolos dengan aapt2, apksigner, dan zipalign asli (format keluaran `aapt2 dump badging` dan daftar library yang dibutuhkan ditebak dari dokumentasi; kalau satu pemeriksaan salah, lognya menunjukkan teks sebenarnya).
2. Pasang APK di HP: apakah aplikasi terbuka (tidak langsung tertutup), tampil landscape layar penuh tanpa area hitam yang salah, sentuhan dan keyboard layar bekerja, suara jalan, dan berhenti saat aplikasi di latar belakang.
3. Periksa `adb logcat` bila gagal: `UnsatisfiedLinkError` berarti library atau simbol yang hilang; "dlopen failed ... page size" berarti alignment 16 KB.
4. Dugaan yang perlu dikonfirmasi saat kompilasi pertama: `minSdk 26` cukup untuk `ALooper_pollOnce` dan audio raylib, dan nama `GetAndroidApp()` yang diekspor raylib.
5. Setelah itu: promosikan job Android menjadi wajib dan jalankan **Release dengan `dry_run = true`** sekali untuk memeriksa kunci rilis.

## Menyalakan leaderboard online (nanti)

Tab Online dimatikan di Android (`ARCADE_ONLINE=OFF` di preset; `app/CMakeLists.txt` memaksanya OFF). Langkah untuk menyalakannya:

1. **libcurl untuk NDK.** `cmake/ArcadeCurl.cmake` membangun curl dari source, tetapi mencari OpenSSL **mesin build** (statis). NDK tidak membawa library TLS, jadi satu harus dibangun untuk arm64 (OpenSSL, mbedTLS, atau BoringSSL) dan diberikan ke curl, atau curl dibangun dengan TLS bawaan pilihan lain.
2. **Sertifikat CA.** `app/net_curl.c` mencari satu file bundle CA (`CA_BUNDLE_PATHS`); Android menyimpannya sebagai folder berkas ber-hash (`/system/etc/security/cacerts`), jadi gunakan `CURLOPT_CAPATH`. Skor tetap tidak boleh dikirim tanpa memeriksa sertifikat server.
3. **Permission.** Tambahkan `<uses-permission android:name="android.permission.INTERNET"/>` di manifest, dan hapus pemeriksaan "tanpa permission" di `tools/verify_apk.sh` dan `tools/tests/test_build_apk.sh`.
4. Hapus penjaga Android di `app/CMakeLists.txt`, ubah `ARCADE_ONLINE` di preset `android-arm64`, tambahkan `libcurl`/TLS ke `ALLOWED_NEEDED` bila berupa library bersama (sebaiknya statis), dan tambahkan lisensinya ke `THIRD_PARTY_NOTICES.md`.

## Batasan dan pekerjaan lanjutan

- **Game `deferred/`:** logika dan tampilan sudah dibangun, tetapi kontrol sentuhnya belum dibuat (satu per satu, mulai dari yang paling sederhana).
- **Bilah navigasi dan notch:** layar penuh memakai bendera jendela dasar; menyembunyikan bilah navigasi sepenuhnya dan memakai area di sekitar notch butuh JNI. Untuk sekarang, Android menahan konten di luar notch (kemungkinan ada pita hitam di sisi).
- **ABI lain** (x86_64 untuk emulator, armeabi-v7a): tambahkan preset dan folder `lib/<abi>/`; `versionCode` perlu skema per-ABI jika APK dipisah.
- **Play Store:** butuh AAB, `targetSdk` terbaru, dan kebijakan data; di luar cakupan sekarang.
- **Latar belakang:** raylib memblokir game selama aplikasi tidak fokus (suara dibisukan oleh `gfx_android.c`), dan game tidak menyimpan keadaan apa pun saat sistem menghentikan aplikasi; skor ditulis seperti di desktop.
