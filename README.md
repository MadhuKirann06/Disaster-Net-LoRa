# Disaster-Net-LoRa
An automated emergency communication framework routing SMS and web alerts via a fallback LoRa mesh topology when the primary GSM cellular network fails.
# Disaster Net: Fail-Safe LoRa & GSM Emergency Communication Framework 🚨

## Project Demo
▶️ [**Click here to watch the Disaster Net Emergency Hardware Demo Video (Google Drive)**](https://drive.google.com/file/d/1Zm4yPzHF7Uxqbu5yh_can_Hj1pqKQA-g/view?usp=sharing)

## Project Description
Disaster Net is an intelligent, dual-layer embedded network engineered to maintain critical emergency communication channels during environmental catastrophes, infrastructure collapses, or localized cellular blackouts. 

The system operates as an automated fallback routing engine. Under nominal conditions, the firmware monitors commercial cellular network infrastructure. Upon tracking a base station connection failure, the controller shifts communication routing seamlessly to an ad-hoc, long-range sub-GHz LoRa topology—preserving text alerts, remote paging, and data transfer capabilities without relying on external cell towers or internet providers.

## System Architecture & Software Implementation

The codebase is split across your network gateway and field nodes to handle automated routing logic:

### 1. Dual-Protocol Intelligent Gateway (`/gateway`)
* **Automated GSM Monitoring:** The firmware periodically runs background checks using the network registration command `AT+CREG?`. If registered (`+CREG: 0,1` or `0,5`), the system defaults to cellular routing. If registration drops, it immediately switches the system flag to LoRa fallback mode.
* **Full GPRS Web Stack:** Includes a fully configured embedded HTTP GET engine. When web access is requested, the code programmatically opens a GPRS bearer link (`AT+SAPBR`), targets a specific access point network (APN: `airtelgprs.com`), initializes an HTTP service session (`AT+HTTPINIT`), executes remote URLs, and captures the string response data directly over the air.
* **SMS & Voice Control Bus:** Listens continuously via a `SoftwareSerial` bus for hardware interrupts like incoming calls (`RING`) or incoming SMS notifications (`+CMTI:`). If in fallback mode, it automatically wraps SMS data into a custom `SMS_FWD:recipient:message` packet string and broadcasts it over the radio layer.

### 2. Field Node & Command Topology (`/field_node_sender` & `/command_center_receiver`)
* **Asynchronous Radio Link:** Deploys remote nodes utilizing high-gain router-style external antennas configured at sub-GHz frequencies ($433\text{ MHz}$ for the core gateway and $915\text{ MHz}$ for independent node nodes) to transmit low-power packets.
* **Telemetry & Handshakes:** The receiver node dynamically parses incoming RF packets, strips out specialized command wrappers (like `WEB_PKT:`), calculates the Received Signal Strength Indication (RSSI) to determine connection health, and sends back automated acknowledgments (`ACK`).

## Project Directory Layout
* `/gateway` — Dynamic control layer regulating automated cellular AT string parsing, GPRS routing, and LoRa handshakes.
* `/field_node_sender` — Remote field transceiver code packaging and routing emergency data.
* `/command_center_receiver` — Base tracking station code capturing and decoding sub-GHz radio telemetry.

## Core Hardware Bill of Materials
* **Processing Architectures:** Multiple Arduino Nano Microcontrollers
* **Cellular Networking Layer:** GSM Transceiver Module (SIM800L Series)
* **Long-Range Sub-GHz Layer:** SX1278 / RFM95 LoRa Radio Transceiver Modules
* **Antenna Infrastructure:** Omnidirectional SMA High-Gain External Router Antennas
