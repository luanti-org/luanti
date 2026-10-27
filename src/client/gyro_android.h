#pragma once

class Camera;

// Inicializa el lector del giroscopio. Devuelve true si tuvo éxito.
bool gyro_init(Camera *camera);

// Lee los eventos pendientes del sensor y aplica los deltas a la cámara.
// Llamar una vez por frame desde el bucle principal.
void gyro_poll(float sensitivity);

// Libera los recursos del sensor. Llamar al cerrar el juego.
void gyro_deinit();
