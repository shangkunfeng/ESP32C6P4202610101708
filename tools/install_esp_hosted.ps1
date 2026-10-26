# Install espressif/esp_hosted into managed_components when Component Manager
# fails on Windows (MAX_PATH) during zip extract.
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Python = "D:\Espressif\tools\python\v6.1\venv\Scripts\python.exe"
if (-not (Test-Path $Python)) {
    Write-Error "IDF Python not found at $Python. Open ESP-IDF terminal or fix path."
}
$env:IDF_COMPONENT_CACHE_PATH = "C:\ec"
New-Item -ItemType Directory -Force -Path "C:\ec" | Out-Null

$code = @"
import shutil, tempfile, importlib.util
from pathlib import Path

proj = Path(r'$ProjectRoot')
dest = proj / 'managed_components' / 'espressif__esp_hosted'
if dest.exists():
    shutil.rmtree(dest, ignore_errors=True)

from idf_component_tools.registry.service_details import get_storage_client
from idf_component_tools.hash_tools.constants import CHECKSUMS_FILENAME
from idf_component_tools.sources.web_service import download_archive, download_file

hash_expected = 'c88052665d8df3c7185cb5e34327d77674599006bbbbe4bfdb43c44f9a2a73a3'
info = get_storage_client('https://components.espressif.com/').component('espressif/esp_hosted', '3.0.10')
tmpdir = tempfile.mkdtemp(dir=r'C:\ec')
archive = download_archive(info['download_url'], tmpdir)

spec = importlib.util.spec_from_file_location('ex', proj / 'tools' / 'extract_esp_hosted_zip.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
mod.unpack_zip_long(archive, str(dest))

checksums = download_file(info['checksums_url'], tmpdir, filename=CHECKSUMS_FILENAME)
shutil.copy2(checksums, dest / CHECKSUMS_FILENAME)
(dest / '.component_hash').write_text(hash_expected, encoding='ascii')
print('esp_hosted installed:', dest)
"@

& $Python -c $code
Write-Host "Done. Run idf.py build from ESP-IDF terminal."
