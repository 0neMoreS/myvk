clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--camera "Camera" ^
--scene A1-GPU-Bottleneck-6.s72 ^
--timer ^
--no-debug ^
--headless ^ < ./report/A1/headless_input/gpu-bottleneck.txt