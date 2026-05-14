#include "Climate.h"
#include "Failsafe.h"
#include "Gui.h"
#include "Mic.h"
#include "Network.h"
#include "Pin.h"
#include "Storage.h"
#include "WiFi.h"
#include "core/Kernel.h"

extern "C" void app_main() {
    static const Kernel::Service* const kManifest[] = {
        &Storage::kService,
        &Failsafe::kService,
        &Pin::kService,
        &Gui::kService,
        &WiFi::kService,
        &Network::kService,
        &Climate::kService,
        &Mic::kService,
        &Mic::kSenderService,
    };

    Kernel::Boot(kManifest, sizeof(kManifest) / sizeof(kManifest[0]));
}
