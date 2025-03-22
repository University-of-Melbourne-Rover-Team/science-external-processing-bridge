#include "generated/assignment.h"
#include "generated/processing.h"
#include <WriteBufferFixedSize.h>
#include <ReadBufferFixedSize.h>
#include <Errors.h>
#include <algorithm>

#include <WiFi.h>
#include <esp_now.h>>

//NOTE: EmbeddedProtobuf needs to be added as a library for this to work
//which the submodule needs to be cloned

//on mac its
//mkdir ~/Documents/Arduino/libraries/EmbeddedProto_Library
//cp EmbeddedProto/src/* ~/Documents/Arduino/libraries/EmbeddedProto_Library

//on linux its
//mkdir ~/Arduino/libraries/EmbeddedProto_Library
//cp EmbeddedProto/src/* ~/Arduino/libraries/EmbeddedProto_Library/

//idk what it is on windows. sorry!

#define DEVKIND DevKind::ScienceExternal

// sending these packets to every esp in range
// hopefully this wont interfere with other teams...
#define BROADCAST_ADDR {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}
uint8_t sender_address[6] = BROADCAST_ADDR;

esp_now_peer_info_t peerInfo {
  .peer_addr = BROADCAST_ADDR,
};


void on_data_send(const uint8_t *mac_addr, esp_now_send_status_t status);
void on_data_recv(const esp_now_recv_info *info, const uint8_t *data, int len);

#define MAX_PKT_SIZE 128
uint8_t transfer_buf[MAX_PKT_SIZE] = {0};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    return;
  }

  esp_now_register_send_cb(on_data_send);

  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_add_peer(&peerInfo)) {
    return;
  }
  esp_now_register_recv_cb(on_data_recv);

  while (Serial.available() < 1) {}
  while (Serial.available() > 0) {
    Serial.read();
  }
  handshake(DEVKIND);
}


EmbeddedProto::WriteBufferFixedSize<10> write_buf;

void handshake(const DevKind& kind) {
  delay(5);
  Assignment asn;
  asn.set_asn(kind);
  write_buf.clear();
  const auto err = asn.serialize(write_buf);

  if (err != EmbeddedProto::Error::NO_ERRORS) {
    // this should never happen
  }

  const auto len = write_buf.get_size();
  const auto bytes = write_buf.get_data();

  #define READ_BUF_SIZE 10
  EmbeddedProto::ReadBufferFixedSize<READ_BUF_SIZE> read_buf;
  Assignment out;

  const auto ser_err = asn.serialize(write_buf);

  if (ser_err != EmbeddedProto::Error::NO_ERRORS) {
    return;
  }

  while (true) {
    auto available_bytes = Serial.available();
    if (available_bytes > 0) {
      read_buf.clear();

      auto buf = read_buf.get_data();
      const auto read = Serial.readBytes(read_buf.get_data(), std::min(available_bytes, READ_BUF_SIZE));
      uint8_t err = 0;
      if(read != available_bytes) {
        err = 1;
        Serial.write(&err, 1);
        continue;
      }
      read_buf.set_bytes_written(available_bytes);

      const auto de_err = out.deserialize(read_buf);
      const auto hack = DevKind::HostAck;

      if (de_err != EmbeddedProto::Error::NO_ERRORS) {
        err = 2;
        Serial.write(&err, 1);
        continue;
      }

      if (out.asn() != hack) {
        err = 3;
        Serial.write(&err, 1);
        continue;
      }

      auto dack = DevKind::DevAck;
      out.set_asn(dack);
      write_buf.clear();
      const auto ack_err = out.serialize(write_buf);
      if (ack_err != EmbeddedProto::Error::NO_ERRORS) {
        err = 4;
        Serial.write(&err, 1);
        continue;
      }

      const auto ack_len = write_buf.get_size();
      const auto ack_bytes = write_buf.get_data();

      Serial.write(ack_bytes, 2);
      delay(10);
      return;
    } else {
      out.set_asn(kind);
      write_buf.clear();
      const auto err = asn.serialize(write_buf);
    }

    Serial.write(bytes, len);
    delay(10);
  }
}

void on_data_send(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    uint8_t send_fail_bytes[1] = {1};
    Serial.write(send_fail_bytes, 1);
  }
}

void on_data_recv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  Serial.write(data, len);
}

size_t last_msg_size = 0;

void loop() {
  const auto available_bytes = Serial.available();
  if (available_bytes > 0) {
    if (available_bytes == 1 && Serial.peek() == 0) {
      handshake(DEVKIND);
      return;
    }

    last_msg_size = Serial.readBytes(transfer_buf, std::min(available_bytes, MAX_PKT_SIZE));
    esp_now_send(sender_address, transfer_buf, last_msg_size);
  } else if (last_msg_size > 0) {
    esp_now_send(sender_address, transfer_buf, last_msg_size);
  }
  delay(10);
}
