#include "protocol.h"
#include <cstring>

namespace KenshiMP {

std::vector<uint8_t> SerializeStateUpdate(uint32_t tick, float world_time,
    const CharacterState* chars, uint32_t count) {

    size_t payload_size = sizeof(StateUpdatePayload) + count * sizeof(CharacterState);
    size_t total = sizeof(PacketHeader) + payload_size;
    if (total > MAX_PACKET_SIZE) return {};

    std::vector<uint8_t> buf(total);
    auto* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    hdr->magic[0] = 'K'; hdr->magic[1] = 'M'; hdr->magic[2] = 'P'; hdr->magic[3] = 0;
    hdr->type = PacketType::StateUpdate;
    hdr->seq = 0;
    hdr->payload_len = static_cast<uint32_t>(payload_size);

    auto* payload = reinterpret_cast<StateUpdatePayload*>(buf.data() + sizeof(PacketHeader));
    payload->tick = tick;
    payload->world_time = world_time;
    payload->character_count = count;

    if (count > 0 && chars) {
        memcpy(payload + 1, chars, count * sizeof(CharacterState));
    }

    return buf;
}

bool DeserializeStateUpdate(const uint8_t* data, size_t len,
    uint32_t& tick, float& world_time,
    CharacterState* chars, uint32_t& count, uint32_t max_chars) {

    if (len < sizeof(PacketHeader) + sizeof(StateUpdatePayload)) return false;

    auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic[0] != 'K' || hdr->magic[1] != 'M' || hdr->magic[2] != 'P') return false;
    if (hdr->type != PacketType::StateUpdate) return false;

    auto* payload = reinterpret_cast<const StateUpdatePayload*>(data + sizeof(PacketHeader));
    tick = payload->tick;
    world_time = payload->world_time;
    count = payload->character_count;

    if (count > max_chars) count = max_chars;

    size_t expected = sizeof(StateUpdatePayload) + count * sizeof(CharacterState);
    if (len < sizeof(PacketHeader) + expected) return false;

    if (count > 0 && chars) {
        memcpy(chars, payload + 1, count * sizeof(CharacterState));
    }

    return true;
}

std::vector<uint8_t> SerializeInputEvent(const InputEventPayload& ev) {
    std::vector<uint8_t> buf(sizeof(PacketHeader) + sizeof(InputEventPayload));
    auto* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    hdr->magic[0] = 'K'; hdr->magic[1] = 'M'; hdr->magic[2] = 'P'; hdr->magic[3] = 0;
    hdr->type = PacketType::InputEvent;
    hdr->seq = 0;
    hdr->payload_len = sizeof(InputEventPayload);
    memcpy(buf.data() + sizeof(PacketHeader), &ev, sizeof(InputEventPayload));
    return buf;
}

std::optional<InputEventPayload> DeserializeInputEvent(const uint8_t* data, size_t len) {
    if (len < sizeof(PacketHeader) + sizeof(InputEventPayload)) return std::nullopt;

    auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic[0] != 'K' || hdr->magic[1] != 'M' || hdr->magic[2] != 'P') return std::nullopt;
    if (hdr->type != PacketType::InputEvent) return std::nullopt;

    InputEventPayload ev;
    memcpy(&ev, data + sizeof(PacketHeader), sizeof(ev));
    return ev;
}

std::vector<uint8_t> SerializeMoneyReport(const MoneyReportPayload& ev) {
    std::vector<uint8_t> buf(sizeof(PacketHeader) + sizeof(MoneyReportPayload));
    auto* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    hdr->magic[0] = 'K'; hdr->magic[1] = 'M'; hdr->magic[2] = 'P'; hdr->magic[3] = 0;
    hdr->type = PacketType::MoneyReport;
    hdr->seq = 0;
    hdr->payload_len = sizeof(MoneyReportPayload);
    memcpy(buf.data() + sizeof(PacketHeader), &ev, sizeof(ev));
    return buf;
}

std::optional<MoneyReportPayload> DeserializeMoneyReport(const uint8_t* data, size_t len) {
    if (len < sizeof(PacketHeader) + sizeof(MoneyReportPayload)) return std::nullopt;

    auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic[0] != 'K' || hdr->magic[1] != 'M' || hdr->magic[2] != 'P') return std::nullopt;
    if (hdr->type != PacketType::MoneyReport) return std::nullopt;

    MoneyReportPayload ev;
    memcpy(&ev, data + sizeof(PacketHeader), sizeof(ev));
    return ev;
}

std::vector<uint8_t> SerializeClientStateReport(uint32_t tick, const CharacterState* chars, uint32_t count) {
    size_t payload_size = sizeof(uint32_t) * 2 + count * sizeof(CharacterState);  // tick + count + chars
    size_t total = sizeof(PacketHeader) + payload_size;
    if (total > MAX_PACKET_SIZE) return {};

    std::vector<uint8_t> buf(total);
    auto* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    hdr->magic[0] = 'K'; hdr->magic[1] = 'M'; hdr->magic[2] = 'P'; hdr->magic[3] = 0;
    hdr->type = PacketType::ClientStateReport;
    hdr->seq = 0;
    hdr->payload_len = static_cast<uint32_t>(payload_size);

    uint8_t* p = buf.data() + sizeof(PacketHeader);
    memcpy(p, &tick, sizeof(tick)); p += sizeof(tick);
    uint32_t cnt = count;
    memcpy(p, &cnt, sizeof(cnt)); p += sizeof(cnt);
    if (count > 0 && chars) {
        memcpy(p, chars, count * sizeof(CharacterState));
    }
    return buf;
}

bool DeserializeClientStateReport(const uint8_t* data, size_t len,
    uint32_t& tick, CharacterState* chars, uint32_t& count, uint32_t max_chars) {

    size_t min_len = sizeof(PacketHeader) + sizeof(uint32_t) * 2;
    if (len < min_len) return false;

    auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic[0] != 'K' || hdr->magic[1] != 'M' || hdr->magic[2] != 'P') return false;
    if (hdr->type != PacketType::ClientStateReport) return false;

    const uint8_t* p = data + sizeof(PacketHeader);
    memcpy(&tick, p, sizeof(tick)); p += sizeof(tick);
    memcpy(&count, p, sizeof(count)); p += sizeof(count);

    if (count > max_chars) count = max_chars;
    size_t expected = min_len + count * sizeof(CharacterState);
    if (len < expected) return false;

    if (count > 0 && chars) {
        memcpy(chars, p, count * sizeof(CharacterState));
    }
    return true;
}

std::vector<uint8_t> SerializeSlotAssign(uint8_t slot) {
    std::vector<uint8_t> buf(sizeof(PacketHeader) + 1);
    auto* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    hdr->magic[0] = 'K'; hdr->magic[1] = 'M'; hdr->magic[2] = 'P'; hdr->magic[3] = 0;
    hdr->type = PacketType::SlotAssign;
    hdr->seq = 0;
    hdr->payload_len = 1;
    buf[sizeof(PacketHeader)] = slot;
    return buf;
}

std::optional<uint8_t> DeserializeSlotAssign(const uint8_t* data, size_t len) {
    if (len < sizeof(PacketHeader) + 1) return std::nullopt;
    auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    if (hdr->magic[0] != 'K' || hdr->magic[1] != 'M' || hdr->magic[2] != 'P') return std::nullopt;
    if (hdr->type != PacketType::SlotAssign) return std::nullopt;
    return data[sizeof(PacketHeader)];
}

std::vector<uint8_t> SerializeWorldReload() {
    std::vector<uint8_t> buf(sizeof(PacketHeader));
    auto* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    hdr->magic[0] = 'K'; hdr->magic[1] = 'M'; hdr->magic[2] = 'P'; hdr->magic[3] = 0;
    hdr->type = PacketType::WorldReload;
    hdr->seq = 0;
    hdr->payload_len = 0;
    return buf;
}

bool IsWorldReloadPacket(const uint8_t* data, size_t len) {
    if (len < sizeof(PacketHeader)) return false;
    auto* hdr = reinterpret_cast<const PacketHeader*>(data);
    return hdr->magic[0] == 'K' && hdr->magic[1] == 'M' && hdr->magic[2] == 'P'
        && hdr->type == PacketType::WorldReload;
}

} // namespace KenshiMP
