//
// Created by Andri Yadi on 18/04/22.
//

#ifndef DXNETWORK_DXESPEVENT_H
#define DXNETWORK_DXESPEVENT_H

#include "sdkconfig.h"
#include <string>

// To avoid including these classes twice when CONFIG_DXNETWORK_USE_EVENT_API_CXX is enabled
#if !CONFIG_DXNETWORK_USE_EVENT_API_CXX

#include <chrono>
const std::chrono::milliseconds PLATFORM_MAX_DELAY_MS(portMAX_DELAY *portTICK_PERIOD_MS);

/**
 * @brief
 * Event ID wrapper class to make C++ APIs more explicit.
 *
 * This prevents APIs from taking raw ints as event IDs which are not very expressive and may be
 * confused with other parameters of a function.
 */
class ESPEventID {
        public:
        ESPEventID() : id(0) { }
        explicit ESPEventID(int32_t event_id): id(event_id) { }
        ESPEventID(const ESPEventID &rhs): id(rhs.id) { }

        inline bool operator==(const ESPEventID &rhs) const {
            return id == rhs.get_id();
        }

        inline ESPEventID &operator=(const ESPEventID& other) {
            id = other.id;
            return *this;
        }

        inline int32_t get_id() const {
            return id;
        }

        friend std::ostream& operator<<(std::ostream& os, const ESPEventID& id);

        private:
        int32_t id;
};

inline std::ostream& operator<<(std::ostream &os, const ESPEventID& id) {
os << id.id;
return os;
}

/*
 * Helper struct to bundle event base and event ID.
 */
struct ESPEvent {
    ESPEvent()
            : base(nullptr), id() { }
    ESPEvent(esp_event_base_t event_base, const ESPEventID &event_id)
    : base(event_base), id(event_id) { }

    esp_event_base_t base;
    ESPEventID id;
};
#endif //!CONFIG_DXNETWORK_USE_EVENT_API_CXX

#endif //DXNETWORK_DXESPEVENT_H
