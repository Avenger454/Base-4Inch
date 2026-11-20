# Frozen Base-4Inch baseline

This repository is the frozen reference for the 91 Firebird display project.

- Hardware-validated firmware commit: `72794a6d8cebcbf384e8cf7f9530619f84403b43`.
- User confirmed normal gauge sweep and correct display on November 20, 2026.
- Target: Waveshare 4-inch ESP32-P4, 720 x 720, ESP-IDF 5.5.6; tested chip revision 3.2.
- Preserve the DSI clock compatibility wrapper and its linker extraction flag.
- Do not modify or push changes to this repository unless the user explicitly authorizes unlocking Base.
- Create SPEEDO, TACH, INFO1, INFO2 and other development work in separate repositories cloned from this baseline.

This file records a development freeze; GitHub archive/protection settings must be applied separately to enforce read-only access.
