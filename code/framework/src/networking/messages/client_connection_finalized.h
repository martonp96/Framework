/*
 * MafiaHub OSS license
 * Copyright (c) 2021, MafiaHub. All rights reserved.
 *
 * This file comes from MafiaHub, hosted at https://github.com/MafiaHub/Framework.
 * See LICENSE file in the source repository for information regarding licensing.
 */

#pragma once

#include "messages.h"

#include <BitStream.h>

namespace Framework::Networking::Messages {
    class ClientConnectionFinalized final: public IMessage {
      private:
        float _serverTickRate = 0.0f;

      public:
        uint8_t GetMessageID() const override {
            return GAME_CONNECTION_FINALIZED;
        }

        void FromParameters(float tickRate) {
            _serverTickRate = tickRate;
        }

        void Serialize(SLNet::BitStream *bs, bool write) override {
            bs->Serialize(write, _serverTickRate);
        }

        bool Valid() override {
            return _serverTickRate > 0.0f;
        }

        float GetServerTickRate() const {
            return _serverTickRate;
        }
    };
} // namespace Framework::Networking::Messages
