#!/usr/bin/env python3
"""
Lighting Protocol HTP Merger — Multi-Universe
===============================================
Receives:
  - Art-Net  on UNIVERSE_COUNT universes starting at ARTNET_START_UNIVERSE
  - sACN     on UNIVERSE_COUNT universes starting at SACN_IN_START_UNIVERSE

Merges each pair using HTP (Highest Takes Precedence) and outputs
the result as sACN on UNIVERSE_COUNT universes starting at SACN_OUT_START_UNIVERSE.

Mapping is 1-to-1 by index offset:
  ArtNet[ARTNET_START + i]  ─┐
                             ├─ HTP ─▶  sACN out[SACN_OUT_START + i]
  sACN  [SACN_IN_START + i] ─┘

Network: 10.0.0.51 / 255.0.0.0  (broadcast: 10.255.255.255)

No external dependencies — pure stdlib.
"""

import socket
import struct
import threading
import time
import logging
import signal
import sys
from typing import Optional

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(threadName)s: %(message)s"
)
log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

LOCAL_IP        = "10.0.0.51"       # This machine's IP
BROADCAST_IP    = "10.255.255.255"  # Subnet broadcast for 10.0.0.0/8

UNIVERSE_COUNT          = 10    # Number of universes to process

ARTNET_START_UNIVERSE   = 11   # First Art-Net universe to receive
SACN_IN_START_UNIVERSE  = 101     # First sACN universe to receive
SACN_OUT_START_UNIVERSE = 1    # First sACN universe to transmit

ARTNET_PORT     = 6454
OUTPUT_IP       = BROADCAST_IP
OUTPUT_FPS      = 44            # Transmit refresh rate (<=44 per E1.31 spec)

SACN_SOURCE_NAME = "HTP Merger"
SACN_PRIORITY    = 100          # E1.31 priority for outgoing streams
ARTNET_TIMEOUT   = 5.0          # Seconds before Art-Net universe goes stale
SACN_TIMEOUT     = 5.0          # Seconds before sACN universe goes stale

DMX_CHANNELS = 512

# ---------------------------------------------------------------------------
# Derived universe lists
# ---------------------------------------------------------------------------

ARTNET_UNIVERSES   = list(range(ARTNET_START_UNIVERSE,   ARTNET_START_UNIVERSE   + UNIVERSE_COUNT))
SACN_IN_UNIVERSES  = list(range(SACN_IN_START_UNIVERSE,  SACN_IN_START_UNIVERSE  + UNIVERSE_COUNT))
SACN_OUT_UNIVERSES = list(range(SACN_OUT_START_UNIVERSE, SACN_OUT_START_UNIVERSE + UNIVERSE_COUNT))

# ---------------------------------------------------------------------------
# Shared state  (one entry per slot index 0..UNIVERSE_COUNT-1)
# ---------------------------------------------------------------------------

artnet_slots:     list = [bytearray(DMX_CHANNELS) for _ in range(UNIVERSE_COUNT)]
sacn_slots:       list = [bytearray(DMX_CHANNELS) for _ in range(UNIVERSE_COUNT)]
artnet_last_seen: list = [0.0] * UNIVERSE_COUNT
sacn_last_seen:   list = [0.0] * UNIVERSE_COUNT

data_lock = threading.Lock()

# Fast lookup: incoming universe number -> slot index
artnet_universe_to_slot  = {u: i for i, u in enumerate(ARTNET_UNIVERSES)}
sacn_in_universe_to_slot = {u: i for i, u in enumerate(SACN_IN_UNIVERSES)}

# ---------------------------------------------------------------------------
# Art-Net receiver
# ---------------------------------------------------------------------------

ARTNET_HEADER = b"Art-Net\x00"
OPCODE_OUTPUT = 0x5000  # ArtDMX


def parse_artnet(data: bytes) -> Optional[tuple]:
    """Parse an Art-Net packet. Returns (universe, dmx_payload) or None."""
    if len(data) < 18:
        return None
    if not data.startswith(ARTNET_HEADER):
        return None
    opcode = struct.unpack_from("<H", data, 8)[0]
    if opcode != OPCODE_OUTPUT:
        return None
    sub_uni  = data[14]
    net      = data[15]
    universe = (net << 8) | sub_uni
    length   = struct.unpack_from(">H", data, 16)[0]
    dmx      = data[18: 18 + length]
    return universe, dmx


def artnet_receiver():
    """Thread: single socket receives all Art-Net universes."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    try:
        sock.bind(("0.0.0.0", ARTNET_PORT))
    except OSError as e:
        log.error(
            f"Cannot bind UDP :{ARTNET_PORT} -- {e}\n"
            f"  Try: sudo lsof -iUDP:{ARTNET_PORT}"
        )
        shutdown_event.set()
        return

    sock.settimeout(1.0)
    log.info(
        f"Art-Net RX  universes {ARTNET_UNIVERSES[0]}-{ARTNET_UNIVERSES[-1]}"
        f"  (port {ARTNET_PORT})"
    )

    while not shutdown_event.is_set():
        try:
            raw, _ = sock.recvfrom(1024)
        except socket.timeout:
            continue
        except OSError:
            break

        result = parse_artnet(raw)
        if result is None:
            continue
        universe, dmx = result

        slot = artnet_universe_to_slot.get(universe)
        if slot is None:
            continue

        with data_lock:
            length = min(len(dmx), DMX_CHANNELS)
            artnet_slots[slot][:length] = dmx[:length]
            artnet_last_seen[slot] = time.monotonic()

    sock.close()
    log.info("Art-Net receiver stopped.")


# ---------------------------------------------------------------------------
# sACN (E1.31) receiver
# ---------------------------------------------------------------------------

def sacn_multicast_addr(universe: int) -> str:
    hi = (universe >> 8) & 0xFF
    lo = universe & 0xFF
    return f"239.255.{hi}.{lo}"


def parse_sacn(data: bytes) -> Optional[tuple]:
    """Parse an E1.31 packet. Returns (universe, dmx_payload) or None."""
    if len(data) < 126:
        return None
    if data[4:16] != b"\x41\x53\x43\x2d\x45\x31\x2e\x31\x37\x00\x00\x00":
        return None
    universe = struct.unpack_from(">H", data, 113)[0]
    if data[125] != 0x00:
        return None
    return universe, data[126:]


def sacn_receiver():
    """Thread: single socket receives all sACN input universes."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, "SO_REUSEPORT"):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    try:
        sock.bind(("0.0.0.0", 5568))
    except OSError as e:
        log.error(
            f"Cannot bind UDP :5568 -- {e}\n"
            "  Try: sudo lsof -iUDP:5568"
        )
        shutdown_event.set()
        return

    # Join multicast group for every input universe
    joined = 0
    for universe in SACN_IN_UNIVERSES:
        mcast = sacn_multicast_addr(universe)
        mreq  = struct.pack("4s4s", socket.inet_aton(mcast), socket.inet_aton(LOCAL_IP))
        try:
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
            joined += 1
        except OSError as e:
            log.warning(f"Could not join multicast {mcast}: {e}")

    log.info(
        f"sACN RX  universes {SACN_IN_UNIVERSES[0]}-{SACN_IN_UNIVERSES[-1]}"
        f"  multicast groups joined: {joined}/{UNIVERSE_COUNT}"
    )

    sock.settimeout(1.0)
    while not shutdown_event.is_set():
        try:
            raw, _ = sock.recvfrom(638)
        except socket.timeout:
            continue
        except OSError:
            break

        result = parse_sacn(raw)
        if result is None:
            continue
        universe, dmx = result

        slot = sacn_in_universe_to_slot.get(universe)
        if slot is None:
            continue

        with data_lock:
            length = min(len(dmx), DMX_CHANNELS)
            sacn_slots[slot][:length] = dmx[:length]
            sacn_last_seen[slot] = time.monotonic()

    sock.close()
    log.info("sACN receiver stopped.")


# ---------------------------------------------------------------------------
# HTP merge
# ---------------------------------------------------------------------------

def htp_merge(a: bytearray, b: bytearray) -> bytearray:
    """Return channel-wise maximum of two DMX arrays."""
    return bytearray(max(x, y) for x, y in zip(a, b))


# ---------------------------------------------------------------------------
# sACN transmitter
# ---------------------------------------------------------------------------

_sacn_seq: list = [0] * UNIVERSE_COUNT


def build_sacn_packet(universe: int, dmx: bytearray, source_name: str, priority: int, seq: int) -> bytes:
    """Build a minimal ANSI E1.31-2018 data packet."""
    prop_count        = len(dmx) + 1
    dmx_layer_len     = (prop_count + 4)          & 0x0FFF | 0x7000
    framing_layer_len = (prop_count + 4 + 77)     & 0x0FFF | 0x7000
    root_layer_len    = (prop_count + 4 + 77 + 4) & 0x0FFF | 0x7000

    src = source_name.encode("utf-8")[:63].ljust(64, b"\x00")
    cid = b"\x68\x74\x70\x2d\x6d\x65\x72\x67\x65\x72\x00\x00\x00\x00\x00\x01"

    pkt = bytearray()
    pkt += b"\x00\x10\x00\x00"
    pkt += b"\x41\x53\x43\x2d\x45\x31\x2e\x31\x37\x00\x00\x00"
    pkt += struct.pack(">H", root_layer_len)
    pkt += b"\x00\x00\x00\x04"
    pkt += cid
    pkt += struct.pack(">H", framing_layer_len)
    pkt += b"\x00\x00\x00\x02"
    pkt += src
    pkt += struct.pack("B", priority)
    pkt += b"\x00\x00"
    pkt += struct.pack("B", seq & 0xFF)
    pkt += b"\x00"
    pkt += struct.pack(">H", universe)
    pkt += struct.pack(">H", dmx_layer_len)
    pkt += b"\x02\xa1\x00\x00\x00\x01"
    pkt += struct.pack(">H", prop_count)
    pkt += b"\x00"
    pkt += bytes(dmx)
    return bytes(pkt)


def sacn_transmitter():
    """Thread: transmit HTP-merged output for all universes at OUTPUT_FPS."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.bind((LOCAL_IP, 0))

    dest_port = 5568
    interval  = 1.0 / OUTPUT_FPS

    log.info(
        f"sACN TX  universes {SACN_OUT_UNIVERSES[0]}-{SACN_OUT_UNIVERSES[-1]}"
        f"  -> {OUTPUT_IP}  @ {OUTPUT_FPS} fps"
    )

    while not shutdown_event.is_set():
        loop_start = time.monotonic()
        now = loop_start

        for i in range(UNIVERSE_COUNT):
            with data_lock:
                an = bytearray(artnet_slots[i])
                sc = bytearray(sacn_slots[i])
                an_stale = (now - artnet_last_seen[i]) > ARTNET_TIMEOUT if artnet_last_seen[i] > 0 else True
                sc_stale = (now - sacn_last_seen[i])   > SACN_TIMEOUT   if sacn_last_seen[i]   > 0 else True

            if an_stale:
                an = bytearray(DMX_CHANNELS)
            if sc_stale:
                sc = bytearray(DMX_CHANNELS)

            merged = htp_merge(an, sc)

            _sacn_seq[i] = (_sacn_seq[i] + 1) & 0xFF
            out_universe  = SACN_OUT_UNIVERSES[i]
            dest_ip       = sacn_multicast_addr(out_universe) if OUTPUT_IP == "multicast" else OUTPUT_IP
            pkt = build_sacn_packet(out_universe, merged, SACN_SOURCE_NAME, SACN_PRIORITY, _sacn_seq[i])

            try:
                sock.sendto(pkt, (dest_ip, dest_port))
            except OSError as e:
                log.error(f"Send error (universe {out_universe}): {e}")

        elapsed = time.monotonic() - loop_start
        sleep_time = interval - elapsed
        if sleep_time > 0:
            time.sleep(sleep_time)

    sock.close()
    log.info("sACN transmitter stopped.")


# ---------------------------------------------------------------------------
# Status logger
# ---------------------------------------------------------------------------

def status_logger():
    """Thread: log per-universe activity summary every 5 seconds."""
    while not shutdown_event.is_set():
        now = time.monotonic()
        lines = []
        for i in range(UNIVERSE_COUNT):
            with data_lock:
                an = bytearray(artnet_slots[i])
                sc = bytearray(sacn_slots[i])
                an_age = now - artnet_last_seen[i] if artnet_last_seen[i] > 0 else None
                sc_age = now - sacn_last_seen[i]   if sacn_last_seen[i]   > 0 else None

            an_ok = an_age is not None and an_age <= ARTNET_TIMEOUT
            sc_ok = sc_age is not None and sc_age <= SACN_TIMEOUT
            peak  = max(htp_merge(
                an if an_ok else bytearray(DMX_CHANNELS),
                sc if sc_ok else bytearray(DMX_CHANNELS)
            ), default=0)
            an_str = f"{an_age:.1f}s" if an_age is not None else "---"
            sc_str = f"{sc_age:.1f}s" if sc_age is not None else "---"
            lines.append(
                f"  slot {i:2d}  AN u{ARTNET_UNIVERSES[i]:4d} [{an_str:>6}]"
                f"  sACN u{SACN_IN_UNIVERSES[i]:4d} [{sc_str:>6}]"
                f"  -> out u{SACN_OUT_UNIVERSES[i]:4d}  peak={peak:3d}"
            )
        log.info("Universe status:\n" + "\n".join(lines))
        time.sleep(5)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

shutdown_event = threading.Event()


def handle_signal(signum, frame):
    log.info("Shutdown signal received.")
    shutdown_event.set()


def main():
    signal.signal(signal.SIGINT,  handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    log.info("=" * 65)
    log.info("  Lighting HTP Merger -- Multi-Universe")
    log.info(f"  Local IP   : {LOCAL_IP}  (bcast {BROADCAST_IP})")
    log.info(f"  Universes  : {UNIVERSE_COUNT}")
    log.info(f"  Art-Net in : u{ARTNET_UNIVERSES[0]} - u{ARTNET_UNIVERSES[-1]}  (port {ARTNET_PORT})")
    log.info(f"  sACN in    : u{SACN_IN_UNIVERSES[0]} - u{SACN_IN_UNIVERSES[-1]}  (port 5568)")
    log.info(f"  sACN out   : u{SACN_OUT_UNIVERSES[0]} - u{SACN_OUT_UNIVERSES[-1]}  -> {OUTPUT_IP}")
    log.info(f"  Merge      : HTP  |  FPS: {OUTPUT_FPS}")
    log.info("=" * 65)

    threads = [
        threading.Thread(target=artnet_receiver,  name="ArtNet-RX",  daemon=True),
        threading.Thread(target=sacn_receiver,    name="sACN-RX",    daemon=True),
        threading.Thread(target=sacn_transmitter, name="sACN-TX",    daemon=True),
        threading.Thread(target=status_logger,    name="StatusLog",  daemon=True),
    ]

    for t in threads:
        t.start()

    while not shutdown_event.is_set():
        time.sleep(0.2)

    log.info("Shutting down...")
    for t in threads:
        t.join(timeout=3.0)
    log.info("Done.")
    sys.exit(0)


if __name__ == "__main__":
    main()