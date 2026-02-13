clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1800 1600 ^
--camera "Camera" ^
--s72-filename sphereflake-blue.s72 ^
--headless ^
--timer ^ < .\report\A1\headless_input\gpu-bottleneck.txt