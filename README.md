# RC//DRIVE

아두이노 나노 + HC-06 RC카용 모바일 우선 조작 화면입니다.

## 실행

브라우저의 Bluetooth/Serial API는 보안 컨텍스트에서만 동작하므로 파일을 직접 여는 대신 로컬 서버로 실행합니다.

```powershell
python -m http.server 8000
```

그 다음 `http://localhost:8000`을 데스크톱 Chrome에서 엽니다. HC-06을 먼저 운영체제에서 페어링한 후 **CONNECT**를 누르고 직렬 포트를 선택하세요. 전송 속도는 아두이노 코드와 동일하게 9600 baud입니다.

조작 명령은 아두이노 코드와 동일합니다.

| 동작 | 전송 문자 |
|---|---|
| 전진 | `1` |
| 후진 | `2` |
| 좌회전 | `3` |
| 우회전 | `4` |
| 정지 | `5` |
| 라인트레이싱 시작 | `6` |
| 라인트레이싱 종료 | `7` |

HC-06은 Bluetooth Classic(SPP)라서 Android/iOS 일반 브라우저의 Web Bluetooth로는 직접 연결되지 않습니다. 휴대폰에서 사용하려면 이 UI를 네이티브 Android 앱(WebView + BluetoothSocket)으로 감싸거나, BLE 모듈(HM-10 등)로 교체해야 합니다. 화면에는 BLE UART 표준 UUID(`FFE0`/`FFE1`) 연결도 포함되어 있습니다.
