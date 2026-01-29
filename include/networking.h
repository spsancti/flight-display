#pragma once

#include "app_types.h"

void networkingInit();
void networkingStartFetchTask();
void networkingEnsureConnected();
bool networkingGetLatest(FlightInfo &out, bool &outValid, uint32_t &outSeq);
