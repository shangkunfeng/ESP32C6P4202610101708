# ESP32C6P4202610101708

ESP-IDF 工程，目标 **esp32p4**（JC1060P470C 类 P4 + C6 WiFi 模组）。

## 构建

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p <PORT> flash monitor
```

## 说明

- 组件依赖见 `main/idf_component.yml`，首次构建会拉取 `managed_components/`（已 gitignore）。
- 厂商 Demo 资料在本地 `资料/` 目录（未上传 GitHub）。
