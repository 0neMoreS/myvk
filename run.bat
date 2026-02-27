clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--camera "Camera" ^
--scene ./report/A2/materials.s72 ^
--no-debug ^
--exposure 0 ^
--tone-map linear
@REM --headless < ./report/A1/headless_input/sphereflake.txt