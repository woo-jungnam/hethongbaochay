#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#if __has_include("firebase_config.h")
#include "firebase_config.h"
#else
#include "firebase_config.example.h"
#endif

const int chanCoi = 25;
const int chanLedDo = 23;
const int chanLedVang = 19;
const int chanLedXanh = 18;
const int chanServo = 26;
const int chanNutKhanCap = 15;

const int nguongKhoiCanhBao = 350;
const int nguongKhoiBaoChay = 650;
const float nguongNhietCanhBao = 50.0;
const float nguongNhietBaoChay = 60.0;
const float nguongGiaNhiet = 3.0;

DHT dht101(33, DHT22);
DHT dht201(27, DHT22);
DHT dht301(14, DHT22);

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoThoatHiem;

FirebaseData duLieuFirebase;
FirebaseData duLieuDieuKhienFirebase;
FirebaseAuth xacThucFirebase;
FirebaseConfig cauHinhFirebase;

struct NutPhong {
  const char* maThietBi;
  const char* tenPhong;
  const char* soPhong;
  const char* viTri;
  int chanKhoi;
  int chanLua;
  int chanDht;
  int chanAmThanh;
  DHT* camBienDht;

  float nhietDo;
  float doAm;
  int nongDoKhoi;
  bool phatHienLua;
  bool phatHienTiengHet;
  float tocDoGiaNhiet;

  float khoiDaLoc;
  float lichSuNhietDo[10];
  int chiSoLichSuNhietDo;

  bool dangBaoChay;
  bool dangCanhBao;
  bool coNguoiKet;
  bool cauCuuKhanCap;
  bool trangThaiBaoChayTruoc;
  bool trangThaiCanhBaoTruoc;
  bool trangThaiNguoiKetTruoc;
  bool trangThaiCauCuuTruoc;
  char lyDo[64];

  bool baoDongThuCong;
  bool canThiepCuaThuCong;
  int gocCuaThuCong;
  bool daTatCoi;
};

NutPhong danhSachPhong[3] = {
  {
    "ESP32_ROOM_101", "Phòng 101 (Deluxe)", "101", "Tầng 1 - Khách Sạn",
    34, 32, 33, 13, &dht101,
    25.0, 60.0, 0, false, false, 0.0,
    0.0, {25,25,25,25,25,25,25,25,25,25}, 0,
    false, false, false, false, false, false, false, false, "Bình thường",
    false, false, 0, false
  },
  {
    "ESP32_ROOM_201", "Phòng 201 (Standard)", "201", "Tầng 2 - Khách Sạn",
    35, 4, 27, 16, &dht201,
    26.0, 58.0, 0, false, false, 0.0,
    0.0, {26,26,26,26,26,26,26,26,26,26}, 0,
    false, false, false, false, false, false, false, false, "Bình thường",
    false, false, 0, false
  },
  {
    "ESP32_ROOM_301", "Phòng 301 (VIP Suite)", "301", "Tầng 3 - Khách Sạn",
    39, 5, 14, 17, &dht301,
    24.5, 55.0, 0, false, false, 0.0,
    0.0, {24.5,24.5,24.5,24.5,24.5,24.5,24.5,24.5,24.5,24.5}, 0,
    false, false, false, false, false, false, false, false, "Bình thường",
    false, false, 0, false
  }
};

bool trangThaiNutKhanCapTruoc = false;
bool dangBaoDongKhanCapToanKhu = false;

unsigned long thoiGianDocCamBienCuoi = 0;
unsigned long thoiGianDocDhtCuoi = 0;
unsigned long thoiGianDongBoFirebaseCuoi = 0;
unsigned long thoiGianKiemTraLenhCuoi = 0;
int chiSoPhongKiemTraLenh = 0;
unsigned long thoiGianChuyenPhongLcdCuoi = 0;
int chiSoPhongLcd = 0;
bool trangThaiChopLed = false;
unsigned long thoiGianChopLedCuoi = 0;

void guiTrangThaiPhongFirebase(NutPhong& phong) {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) return;

  const char* chuoiTrangThai = (phong.dangBaoChay || phong.baoDongThuCong) ? "FIRE_ALARM" : (phong.dangCanhBao ? "SMOKE_WARN" : (phong.cauCuuKhanCap ? "PANIC_SOS" : "SAFE"));
  const char* mucUuTien = phong.coNguoiKet ? "P1_URGENT" : ((phong.dangBaoChay || phong.baoDongThuCong) ? "P2_FIRE" : (phong.cauCuuKhanCap ? "P3_SECURITY" : "NORMAL"));
  int gocServo = phong.canThiepCuaThuCong ? phong.gocCuaThuCong : ((phong.dangBaoChay || phong.baoDongThuCong || phong.coNguoiKet) ? 90 : 0);
  int trangThaiCoi = (!phong.daTatCoi && (phong.dangBaoChay || phong.baoDongThuCong || phong.dangCanhBao || phong.coNguoiKet || phong.cauCuuKhanCap)) ? 1 : 0;

  FirebaseJson json;
  json.set("device_id", phong.maThietBi);
  json.set("name", phong.tenPhong);
  json.set("room_number", phong.soPhong);
  json.set("location", phong.viTri);
  json.set("smoke_ppm", phong.nongDoKhoi);
  json.set("temperature", phong.nhietDo);
  json.set("humidity", phong.doAm);
  json.set("flame_detected", phong.phatHienLua ? 1 : 0);
  json.set("scream_detected", phong.phatHienTiengHet ? 1 : 0);
  json.set("human_trapped", phong.coNguoiKet ? 1 : 0);
  json.set("panic_sos", phong.cauCuuKhanCap ? 1 : 0);
  json.set("rescue_priority", mucUuTien);
  json.set("rate_of_rise", phong.tocDoGiaNhiet);
  json.set("status", chuoiTrangThai);
  json.set("servo_angle", gocServo);
  json.set("buzzer_active", trangThaiCoi);
  json.set("alarm_reason", phong.lyDo);
  json.set("ip_address", WiFi.localIP().toString().c_str());
  json.set("last_seen/.sv", "timestamp");

  String duongDan = String("/devices/") + phong.maThietBi + "/state";
  Firebase.RTDB.updateNode(&duLieuFirebase, duongDan.c_str(), &json);
}

void dayCanhBaoPhongFirebase(NutPhong& phong, const char* loaiCanhBao, const char* mucDo, const char* tieuDe) {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) return;

  FirebaseJson canhBao;
  canhBao.set("device_id", phong.maThietBi);
  canhBao.set("room_name", phong.tenPhong);
  canhBao.set("room_number", phong.soPhong);
  canhBao.set("location", phong.viTri);
  canhBao.set("alert_type", loaiCanhBao);
  canhBao.set("severity", mucDo);
  canhBao.set("title", tieuDe);
  canhBao.set("message", phong.lyDo);
  canhBao.set("smoke_ppm", phong.nongDoKhoi);
  canhBao.set("temperature", phong.nhietDo);
  canhBao.set("humidity", phong.doAm);
  canhBao.set("flame_detected", phong.phatHienLua ? 1 : 0);
  canhBao.set("scream_detected", phong.phatHienTiengHet ? 1 : 0);
  canhBao.set("human_trapped", phong.coNguoiKet ? 1 : 0);
  canhBao.set("panic_sos", phong.cauCuuKhanCap ? 1 : 0);
  canhBao.set("is_acknowledged", false);
  canhBao.set("is_resolved", false);
  canhBao.set("created_at/.sv", "timestamp");

  if (Firebase.RTDB.pushJSON(&duLieuFirebase, "/alerts", &canhBao)) {
    Serial.printf("[FIREBASE ALERT] [%s] %s: %s\n", phong.tenPhong, tieuDe, phong.lyDo);
  }
}

void kiemTraLenhPhongFirebase(NutPhong& phong) {
  if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) return;

  String duongDan = String("/devices/") + phong.maThietBi + "/control/command";
  if (Firebase.RTDB.getString(&duLieuDieuKhienFirebase, duongDan.c_str())) {
    String lenh = duLieuDieuKhienFirebase.stringData();
    if (lenh.length() > 0 && lenh != "NONE" && lenh != "\"NONE\"") {
      lenh.replace("\"", "");
      Serial.printf("[CMD] [%s] Nhan lenh: %s\n", phong.tenPhong, lenh.c_str());

      if (lenh == "TEST_ALARM") {
        phong.baoDongThuCong = true;
        phong.dangBaoChay = true;
        phong.daTatCoi = false;
        dayCanhBaoPhongFirebase(phong, "SYSTEM_TEST", "CRITICAL", "LỆNH BÁO ĐỘNG SƠ TÁN PHÒNG");
      } else if (lenh == "RESET_SAFE") {
        bool vanConNguyHiem = (phong.phatHienLua || phong.nongDoKhoi >= nguongKhoiBaoChay || phong.nhietDo >= nguongNhietBaoChay);
        if (!vanConNguyHiem) {
          phong.dangBaoChay = false;
          phong.dangCanhBao = false;
          phong.coNguoiKet = false;
          phong.cauCuuKhanCap = false;
          phong.baoDongThuCong = false;
          phong.daTatCoi = false;
          phong.canThiepCuaThuCong = false;
          dangBaoDongKhanCapToanKhu = false;
          snprintf(phong.lyDo, sizeof(phong.lyDo), "Bình thường");
          dayCanhBaoPhongFirebase(phong, "SAFE_RESTORED", "INFO", "KHÔI PHỤC AN TOÀN PHÒNG");
        } else {
          Serial.printf("[CMD] [%s] Khong the dat lai: Hien truong van con nguy hiem!\n", phong.tenPhong);
        }
      } else if (lenh == "OPEN_EVACUATION") {
        phong.canThiepCuaThuCong = true;
        phong.gocCuaThuCong = 90;
      } else if (lenh == "CLOSE_EVACUATION") {
        phong.canThiepCuaThuCong = true;
        phong.gocCuaThuCong = 0;
      } else if (lenh == "MUTE_BUZZER") {
        phong.daTatCoi = true;
      }

      Firebase.RTDB.setString(&duLieuDieuKhienFirebase, duongDan.c_str(), "NONE");
      guiTrangThaiPhongFirebase(phong);
    }
  }
}

void docCamBienPhong(NutPhong& phong, bool docDht = false) {
  int giaTriKhoiTho = analogRead(phong.chanKhoi);
  int khoiQuyDoi = map(giaTriKhoiTho, 0, 4095, 0, 1000);
  if (phong.khoiDaLoc == 0.0) {
    phong.khoiDaLoc = khoiQuyDoi;
  } else {
    phong.khoiDaLoc = 0.3f * khoiQuyDoi + 0.7f * phong.khoiDaLoc;
  }
  phong.nongDoKhoi = (int)(phong.khoiDaLoc + 0.5f);

  if (phong.chanLua >= 0) {
    phong.phatHienLua = (digitalRead(phong.chanLua) == HIGH);
  } else {
    phong.phatHienLua = false;
  }

  phong.phatHienTiengHet = (digitalRead(phong.chanAmThanh) == HIGH);

  if (docDht) {
    float t = phong.camBienDht->readTemperature();
    float h = phong.camBienDht->readHumidity();
    if (!isnan(t)) phong.nhietDo = t;
    if (!isnan(h)) phong.doAm = h;

    float nhietDoCu = phong.lichSuNhietDo[phong.chiSoLichSuNhietDo];
    phong.lichSuNhietDo[phong.chiSoLichSuNhietDo] = phong.nhietDo;
    phong.chiSoLichSuNhietDo = (phong.chiSoLichSuNhietDo + 1) % 10;
    phong.tocDoGiaNhiet = phong.nhietDo - nhietDoCu;
  }

  bool baoChayGiaNhiet = (phong.tocDoGiaNhiet >= nguongGiaNhiet && phong.nhietDo >= 38.0);

  bool ungVienChay = false;
  char lyDoChay[64] = "";
  if (phong.phatHienLua) {
    ungVienChay = true;
    snprintf(lyDoChay, sizeof(lyDoChay), "Phát hiện ngọn lửa");
  } else if (baoChayGiaNhiet) {
    ungVienChay = true;
    snprintf(lyDoChay, sizeof(lyDoChay), "Nhiệt tăng nhanh (+%.1f°C/10s, %.1f°C)", phong.tocDoGiaNhiet, phong.nhietDo);
  } else if (phong.nhietDo >= nguongNhietBaoChay) {
    ungVienChay = true;
    snprintf(lyDoChay, sizeof(lyDoChay), "Nhiệt độ tới hạn (%.1f°C)", phong.nhietDo);
  } else if (phong.nongDoKhoi >= nguongKhoiBaoChay) {
    ungVienChay = true;
    snprintf(lyDoChay, sizeof(lyDoChay), "Khói dày đặc (%d PPM)", phong.nongDoKhoi);
  } else if (phong.nongDoKhoi >= 400 && phong.nhietDo >= 48.0) {
    ungVienChay = true;
    snprintf(lyDoChay, sizeof(lyDoChay), "Kết hợp khói %dPPM & nhiệt %.1f°C", phong.nongDoKhoi, phong.nhietDo);
  }

  if (ungVienChay) {
    phong.dangBaoChay = true;
    phong.dangCanhBao = false;
    strncpy(phong.lyDo, lyDoChay, sizeof(phong.lyDo) - 1);
  }

  if (phong.dangBaoChay || phong.baoDongThuCong) {
    if (phong.phatHienTiengHet) {
      phong.coNguoiKet = true;
      snprintf(phong.lyDo, sizeof(phong.lyDo), "PHÁT HIỆN TIẾNG LA HÉT SOS! NẠN NHÂN MẮC KẸT");
    }
    phong.cauCuuKhanCap = false;
  } else {
    if (phong.phatHienTiengHet) {
      phong.cauCuuKhanCap = true;
      snprintf(phong.lyDo, sizeof(phong.lyDo), "TIẾNG HÉT CẦU CỨU KHẨN CẤP (CHƯA PHÁT HIỆN CHÁY)");
    } else {
      phong.cauCuuKhanCap = false;
    }

    if (phong.nongDoKhoi >= nguongKhoiCanhBao || phong.nhietDo >= nguongNhietCanhBao) {
      phong.dangCanhBao = true;
      if (phong.nongDoKhoi >= nguongKhoiCanhBao) {
        snprintf(phong.lyDo, sizeof(phong.lyDo), "Nồng độ khói tăng (%d PPM)", phong.nongDoKhoi);
      } else {
        snprintf(phong.lyDo, sizeof(phong.lyDo), "Nhiệt độ cao (%.1f°C)", phong.nhietDo);
      }
    } else if (phong.dangCanhBao) {
      if (phong.nongDoKhoi < 280 && phong.nhietDo < 45.0) {
        phong.dangCanhBao = false;
        if (!phong.cauCuuKhanCap) snprintf(phong.lyDo, sizeof(phong.lyDo), "Bình thường");
      }
    } else {
      if (!phong.cauCuuKhanCap) snprintf(phong.lyDo, sizeof(phong.lyDo), "Bình thường");
    }
  }

  if (phong.coNguoiKet && !phong.trangThaiNguoiKetTruoc) {
    guiTrangThaiPhongFirebase(phong);
    dayCanhBaoPhongFirebase(phong, "HUMAN_TRAPPED", "CRITICAL", "CẢNH BÁO CÓ NGƯỜI KẸT - ƯU TIÊN CỨU HỘ SỐ 1!");
  } else if (!phong.coNguoiKet && phong.trangThaiNguoiKetTruoc) {
    guiTrangThaiPhongFirebase(phong);
  }
  phong.trangThaiNguoiKetTruoc = phong.coNguoiKet;

  if (phong.cauCuuKhanCap && !phong.trangThaiCauCuuTruoc && !phong.dangBaoChay) {
    guiTrangThaiPhongFirebase(phong);
    dayCanhBaoPhongFirebase(phong, "PANIC_SOS", "WARNING", "CẦU CỨU KHẨN CẤP (CHƯA PHÁT HIỆN CHÁY)");
  }
  phong.trangThaiCauCuuTruoc = phong.cauCuuKhanCap;

  if (phong.dangBaoChay != phong.trangThaiBaoChayTruoc || phong.dangCanhBao != phong.trangThaiCanhBaoTruoc) {
    guiTrangThaiPhongFirebase(phong);
    if (phong.dangBaoChay && !phong.trangThaiBaoChayTruoc) {
      dayCanhBaoPhongFirebase(phong, "FIRE_ALARM", "CRITICAL", "BÁO ĐỘNG HỎA HOẠN");
    } else if (phong.dangCanhBao && !phong.trangThaiCanhBaoTruoc && !phong.dangBaoChay) {
      dayCanhBaoPhongFirebase(phong, "SMOKE_WARN", "WARNING", "CẢNH BÁO NỒNG ĐỘ KHÓI");
    } else if (!phong.dangBaoChay && !phong.dangCanhBao && !phong.cauCuuKhanCap) {
      dayCanhBaoPhongFirebase(phong, "SAFE_RESTORED", "INFO", "KHÔI PHỤC AN TOÀN");
    }
  }
  phong.trangThaiBaoChayTruoc = phong.dangBaoChay;
  phong.trangThaiCanhBaoTruoc = phong.dangCanhBao;
}

void capNhatCoCauChapHanh() {
  bool coBaoChay = false;
  bool coCanhBao = false;
  bool coNguoiKet = false;
  bool coCauCuu = false;
  int chiSoPhongNguoiKet = -1;
  int chiSoPhongBaoChay = -1;
  int chiSoPhongCanhBao = -1;
  int chiSoPhongCauCuu = -1;

  bool coNguoiKetCanCoi = false;
  bool coBaoChayCanCoi = false;
  bool coCanhBaoCanCoi = false;
  bool coCauCuuCanCoi = false;

  for (int i = 0; i < 3; i++) {
    if (danhSachPhong[i].coNguoiKet) {
      coNguoiKet = true;
      chiSoPhongNguoiKet = i;
      if (!danhSachPhong[i].daTatCoi) coNguoiKetCanCoi = true;
    }
    if (danhSachPhong[i].dangBaoChay || danhSachPhong[i].baoDongThuCong) {
      coBaoChay = true;
      if (chiSoPhongBaoChay < 0) chiSoPhongBaoChay = i;
      if (!danhSachPhong[i].daTatCoi) coBaoChayCanCoi = true;
    } else if (danhSachPhong[i].dangCanhBao) {
      coCanhBao = true;
      if (chiSoPhongCanhBao < 0) chiSoPhongCanhBao = i;
      if (!danhSachPhong[i].daTatCoi) coCanhBaoCanCoi = true;
    } else if (danhSachPhong[i].cauCuuKhanCap) {
      coCauCuu = true;
      if (chiSoPhongCauCuu < 0) chiSoPhongCauCuu = i;
      if (!danhSachPhong[i].daTatCoi) coCauCuuCanCoi = true;
    }
  }

  if (millis() - thoiGianChopLedCuoi > (coNguoiKet ? 120 : (coBaoChay ? 250 : 400))) {
    thoiGianChopLedCuoi = millis();
    trangThaiChopLed = !trangThaiChopLed;
  }

  digitalWrite(chanLedDo, coBaoChay ? (trangThaiChopLed ? HIGH : LOW) : LOW);
  digitalWrite(chanLedVang, (!coBaoChay && (coCanhBao || coCauCuu)) ? (trangThaiChopLed ? HIGH : LOW) : LOW);
  digitalWrite(chanLedXanh, (!coBaoChay && !coCanhBao && !coCauCuu) ? HIGH : LOW);

  if (coNguoiKetCanCoi) {
    if ((millis() / 150) % 2 == 0) {
      tone(chanCoi, 1500);
    } else {
      tone(chanCoi, 900);
    }
  } else if (coBaoChayCanCoi) {
    if ((millis() / 300) % 2 == 0) {
      tone(chanCoi, 1200);
    } else {
      tone(chanCoi, 800);
    }
  } else if (coCanhBaoCanCoi) {
    if ((millis() / 500) % 2 == 0) {
      tone(chanCoi, 1000);
    } else {
      noTone(chanCoi);
    }
  } else if (coCauCuuCanCoi) {
    if ((millis() / 250) % 2 == 0) {
      tone(chanCoi, 800);
    } else {
      noTone(chanCoi);
    }
  } else {
    noTone(chanCoi);
  }

  bool coCanThiepCua = false;
  int gocCanThiep = 0;
  for (int i = 0; i < 3; i++) {
    if (danhSachPhong[i].canThiepCuaThuCong) {
      coCanThiepCua = true;
      if (danhSachPhong[i].gocCuaThuCong > gocCanThiep) {
        gocCanThiep = danhSachPhong[i].gocCuaThuCong;
      }
    }
  }

  if (coCanThiepCua) {
    servoThoatHiem.write(gocCanThiep);
  } else if (coNguoiKet || coBaoChay) {
    servoThoatHiem.write(90);
  } else {
    servoThoatHiem.write(0);
  }

  if (dangBaoDongKhanCapToanKhu) {
    lcd.setCursor(0, 0);
    lcd.print("! EMERGENCY SOS !");
    lcd.setCursor(0, 1);
    lcd.print("EVACUATE NOW !!");
  } else if (coNguoiKet && chiSoPhongNguoiKet >= 0) {
    lcd.setCursor(0, 0);
    char dong1[17];
    snprintf(dong1, sizeof(dong1), "!SOS: P.%s TRAP!", danhSachPhong[chiSoPhongNguoiKet].soPhong);
    lcd.print(dong1);
    lcd.setCursor(0, 1);
    char dong2[17];
    snprintf(dong2, sizeof(dong2), "RESCUE P1: T%.0fC", danhSachPhong[chiSoPhongNguoiKet].nhietDo);
    lcd.print(dong2);
  } else if (coBaoChay && chiSoPhongBaoChay >= 0) {
    lcd.setCursor(0, 0);
    char dong1[17];
    snprintf(dong1, sizeof(dong1), "!! FIRE: %s !!", danhSachPhong[chiSoPhongBaoChay].soPhong);
    lcd.print(dong1);
    lcd.setCursor(0, 1);
    char dong2[17];
    snprintf(dong2, sizeof(dong2), "S:%3d T:%.0fC CRIT", danhSachPhong[chiSoPhongBaoChay].nongDoKhoi, danhSachPhong[chiSoPhongBaoChay].nhietDo);
    lcd.print(dong2);
  } else if (coCauCuu && chiSoPhongCauCuu >= 0) {
    lcd.setCursor(0, 0);
    char dong1[17];
    snprintf(dong1, sizeof(dong1), "! PANIC: P.%s !", danhSachPhong[chiSoPhongCauCuu].soPhong);
    lcd.print(dong1);
    lcd.setCursor(0, 1);
    lcd.print("NEED HELP SOS ! ");
  } else if (coCanhBao && chiSoPhongCanhBao >= 0) {
    lcd.setCursor(0, 0);
    char dong1[17];
    snprintf(dong1, sizeof(dong1), "! WARN: P.%s ! ", danhSachPhong[chiSoPhongCanhBao].soPhong);
    lcd.print(dong1);
    lcd.setCursor(0, 1);
    char dong2[17];
    snprintf(dong2, sizeof(dong2), "S:%3d T:%.0fC", danhSachPhong[chiSoPhongCanhBao].nongDoKhoi, danhSachPhong[chiSoPhongCanhBao].nhietDo);
    lcd.print(dong2);
  } else {
    if (millis() - thoiGianChuyenPhongLcdCuoi > 2500) {
      thoiGianChuyenPhongLcdCuoi = millis();
      chiSoPhongLcd = (chiSoPhongLcd + 1) % 3;
    }
    NutPhong& phongHienTai = danhSachPhong[chiSoPhongLcd];
    lcd.setCursor(0, 0);
    char dong1[17];
    snprintf(dong1, sizeof(dong1), "HOTEL P.%s: SAFE", phongHienTai.soPhong);
    lcd.print(dong1);
    lcd.setCursor(0, 1);
    char dong2[17];
    snprintf(dong2, sizeof(dong2), "S:%2d T:%.0fC H:%.0f%%", phongHienTai.nongDoKhoi, phongHienTai.nhietDo, phongHienTai.doAm);
    lcd.print(dong2);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("=================================================");
  Serial.println(" HE THONG BAO CHAY & DINH VI CUU HO KHACH SAN");
  Serial.println(" Tich hop cam bien phat hien tieng het cau cuu");
  Serial.println("=================================================");

  pinMode(chanCoi, OUTPUT);
  pinMode(chanLedDo, OUTPUT);
  pinMode(chanLedVang, OUTPUT);
  pinMode(chanLedXanh, OUTPUT);

  pinMode(danhSachPhong[0].chanKhoi, INPUT);
  pinMode(danhSachPhong[0].chanLua, INPUT_PULLDOWN);
  pinMode(danhSachPhong[0].chanAmThanh, INPUT_PULLDOWN);

  pinMode(danhSachPhong[1].chanKhoi, INPUT);
  pinMode(danhSachPhong[1].chanLua, INPUT_PULLDOWN);
  pinMode(danhSachPhong[1].chanAmThanh, INPUT_PULLDOWN);

  pinMode(danhSachPhong[2].chanKhoi, INPUT);
  pinMode(danhSachPhong[2].chanLua, INPUT_PULLDOWN);
  pinMode(danhSachPhong[2].chanAmThanh, INPUT_PULLDOWN);
  pinMode(chanNutKhanCap, INPUT_PULLDOWN);

  servoThoatHiem.setPeriodHertz(50);
  servoThoatHiem.attach(chanServo, 500, 2400);
  servoThoatHiem.write(0);

  dht101.begin();
  dht201.begin();
  dht301.begin();

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HOTEL FIRE ALARM");
  lcd.setCursor(0, 1);
  lcd.print("RESCUE SYSTEM OK");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Dang ket noi WiFi (\"%s\")", WIFI_SSID);
  unsigned long batDauWifi = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - batDauWifi < 10000)) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi da ket noi!");
  } else {
    Serial.println("\n[CANH BAO] Khong co WiFi! He thong chay offline che do cuc bo.");
  }

  cauHinhFirebase.database_url = FIREBASE_HOST;
  cauHinhFirebase.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&cauHinhFirebase, &xacThucFirebase);
  Firebase.reconnectWiFi(true);
  duLieuFirebase.setResponseSize(2048);
  duLieuDieuKhienFirebase.setResponseSize(1024);

  for (int i = 0; i < 3; i++) {
    docCamBienPhong(danhSachPhong[i], true);
    guiTrangThaiPhongFirebase(danhSachPhong[i]);
  }
}

void loop() {
  unsigned long thoiGianHienTai = millis();

  bool docDht = false;
  if (thoiGianHienTai - thoiGianDocDhtCuoi >= 1000) {
    thoiGianDocDhtCuoi = thoiGianHienTai;
    docDht = true;
  }

  if (thoiGianHienTai - thoiGianDocCamBienCuoi >= 300) {
    thoiGianDocCamBienCuoi = thoiGianHienTai;
    for (int i = 0; i < 3; i++) {
      docCamBienPhong(danhSachPhong[i], docDht);
    }
  }

  if (thoiGianHienTai - thoiGianDongBoFirebaseCuoi >= 2000) {
    thoiGianDongBoFirebaseCuoi = thoiGianHienTai;
    for (int i = 0; i < 3; i++) {
      guiTrangThaiPhongFirebase(danhSachPhong[i]);
    }
    Serial.printf("[TELEMETRY] P.101: %d ppm, %.1fC | P.201: %d ppm, %.1fC | P.301: %d ppm, %.1fC\n",
      danhSachPhong[0].nongDoKhoi, danhSachPhong[0].nhietDo,
      danhSachPhong[1].nongDoKhoi, danhSachPhong[1].nhietDo,
      danhSachPhong[2].nongDoKhoi, danhSachPhong[2].nhietDo
    );
  }

  if (thoiGianHienTai - thoiGianKiemTraLenhCuoi >= 400) {
    thoiGianKiemTraLenhCuoi = thoiGianHienTai;
    kiemTraLenhPhongFirebase(danhSachPhong[chiSoPhongKiemTraLenh]);
    chiSoPhongKiemTraLenh = (chiSoPhongKiemTraLenh + 1) % 3;
  }

  bool nutNhanKhanCap = (digitalRead(chanNutKhanCap) == HIGH);
  if (nutNhanKhanCap && !trangThaiNutKhanCapTruoc) {
    dangBaoDongKhanCapToanKhu = true;
    for (int i = 0; i < 3; i++) {
      danhSachPhong[i].baoDongThuCong = true;
      danhSachPhong[i].dangBaoChay = true;
      danhSachPhong[i].daTatCoi = false;
      snprintf(danhSachPhong[i].lyDo, sizeof(danhSachPhong[i].lyDo), "NÚT BÁO CHÁY KHẨN CẤP ĐÃ ĐƯỢC KÍCH HOẠT!");
      guiTrangThaiPhongFirebase(danhSachPhong[i]);
      dayCanhBaoPhongFirebase(danhSachPhong[i], "MANUAL_CALL_POINT", "CRITICAL", "KÍCH HOẠT NÚT BÁO CHÁY KHẨN CẤP");
    }
  }
  trangThaiNutKhanCapTruoc = nutNhanKhanCap;

  capNhatCoCauChapHanh();

  delay(10);
}
