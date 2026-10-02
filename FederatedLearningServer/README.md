# FederatedLearningServer


This is the Federated Server responsible for merge and distribute the global model for the ESP32 Clients for the training.

The directory src have the necessary files and the lib directory is the .h correlated.

The Makefile is configured as follows:
    * make: compile and generated the program 
    * make clean: force remove of .o files inside the .build directory and the program file
    * make run: execute the program file

## Phases (teacher training → federated training)

The server runs in two phases once `CLIENTS_NUM` clients are registered:

1. **TEACHER** — if a client registered with `GET /api/noderegister?role=teacher` (the simulated client in
   `../FederatedLearningTeacherClient`), the server sends it an untrained teacher (`GET /api/getteachertask`,
   architecture from `TEACHER_*` in `lib/configs.h`). The client trains it and sends it back through the
   WebSocket with `"type": "teacher"`. The trained teacher is saved as `modelos/<run>/teacher.json` and its
   metrics go to `metrics.csv` with `interaction = -1`. After `TEACHER_TIMEOUT_S` without a teacher the
   server moves on without one.
2. **FEDERATED** — the usual rounds. The teacher is served at `GET /api/getteachermodel` together with the
   distillation parameters (`KD_TEMPERATURE`, `KD_ALPHA`), and the teacher client takes part as a regular
   client.

`GET /api/checkglobalmodel` now answers `{"status", "round", "phase", "task"}`; in phase TEACHER only the
teacher client gets `status: 1` (`task: "teacher"`). Without a teacher client the server starts the
federated phase directly.
