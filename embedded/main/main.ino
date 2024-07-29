#include "generated/assignment.h"
#include <WriteBufferFixedSize.h>
#include <ReadBufferFixedSize.h>
#include <Errors.h>

//NOTE: EmbeddedProtobuf needs to be added as a library for this to work
//which the submodule needs to be cloned

//on mac its
//mkdir ~/Documents/Arduino/libraries/EmbeddedProto_Library
//cp EmbeddedProto/src/* ~/Documents/Arduino/libraries/EmbeddedProto_Library

//on linux its
//mkdir ~/Arduino/libraries/EmbeddedProto_Library
//cp EmbeddedProto/src/* ~/Arduino/libraries/EmbeddedProto_Library/

//idk what it is on windows. sorry!

#define DEVKIND DevKind::Battery

void setup() {
  Serial.begin(115200);
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

  EmbeddedProto::ReadBufferFixedSize<10> read_buf;
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
      const auto read = Serial.readBytes(read_buf.get_data(), available_bytes);
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

uint8_t v = 0;

void loop() {
  const auto available_bytes = Serial.available();
  if (available_bytes > 0) {
    if (available_bytes == 1 && Serial.peek() == 0) {
      handshake(DEVKIND);
      return;
    }

    // do input processing here
  }

  // const auto bytes = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};


  Serial.write(&v, 1);
  delay(10);
}