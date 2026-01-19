# Arduino LED/Touch Controller Protocol Manual

## Overview

This document describes the serial communication protocol between a Raspberry Pi (logic controller) and an Arduino UNO R4 WiFi (hardware controller). The Arduino manages 25 LED positions (A-Y) and 25 capacitive touch sensors, while the Pi implements game logic.

**Architecture:**
```
┌─────────────────┐     Serial (115200 baud)     ┌─────────────────┐
│  Raspberry Pi   │ ◄──────────────────────────► │    Arduino      │
│  (Game Logic)   │      Text Commands/Events    │  (Hardware I/O) │
└─────────────────┘                              └─────────────────┘
```

**Protocol Version:** 2.0  
**Baud Rate:** 115200  
**Line Terminator:** `\n` (newline)

---

## Position Identifiers

Positions are identified by letters A-Y (25 positions total):
- Position A = index 0
- Position B = index 1
- ...
- Position Y = index 24

Case-insensitive: `A` and `a` are equivalent.

---

## Command Format

```
COMMAND [position] [#command_id]
```

- **COMMAND**: Action name (case-insensitive)
- **position**: Single letter A-Y (required for some commands)
- **#command_id**: Optional numeric ID for tracking (e.g., `#1001`)

### Example Commands
```
SHOW A
SHOW B #1001
HIDE A #1002
SUCCESS C #1003
```

---

## Commands Reference

### LED Commands

| Command | Syntax | Description | Response |
|---------|--------|-------------|----------|
| `SHOW` | `SHOW <pos> [#id]` | Light LED at position (blue) | `ACK SHOW <pos> [#id]` |
| `HIDE` | `HIDE <pos> [#id]` | Turn off LED at position | `ACK HIDE <pos> [#id]` |
| `SUCCESS` | `SUCCESS <pos> [#id]` | Play green expansion animation | `ACK ...` then `DONE SUCCESS <pos> [#id]` |
| `BLINK` | `BLINK <pos> [#id]` | Start blinking LED (orange, fast) - signals "release me" | `ACK BLINK <pos> [#id]` |
| `STOP_BLINK` | `STOP_BLINK <pos> [#id]` | Stop blinking, turn off LED | `ACK STOP_BLINK <pos> [#id]` |
| `SEQUENCE_COMPLETED` | `SEQUENCE_COMPLETED [#id]` | Play celebration animation on all LEDs | `ACK ...` then `DONE SEQUENCE_COMPLETED [#id]` |

### Touch Commands

| Command | Syntax | Description | Response |
|---------|--------|-------------|----------|
| `EXPECT_DOWN` | `EXPECT_DOWN <pos> [#id]` | Wait for touch at position | `ACK ...` then `TOUCHED_DOWN <pos> [#id]` when touched |
| `EXPECT_UP` | `EXPECT_UP <pos> [#id]` | Wait for release at position | `ACK ...` then `TOUCHED_UP <pos> [#id]` when released |
| `RECALIBRATE` | `RECALIBRATE <pos> [#id]` | Recalibrate single sensor | `ACK ...` then `RECALIBRATED <pos> [#id]` |
| `RECALIBRATE_ALL` | `RECALIBRATE_ALL [#id]` | Recalibrate all sensors | `ACK ...` then `RECALIBRATED ALL [#id]` |

### Utility Commands

| Command | Syntax | Description | Response |
|---------|--------|-------------|----------|
| `PING` | `PING [#id]` | Check connection | `ACK PING [#id]` |
| `INFO` | `INFO [#id]` | Get firmware info | `INFO firmware=2.0.0 protocol=2 [#id]` |
| `SCAN` | `SCAN [#id]` | Scan for connected touch sensors | `SCANNED [A,B,C,...] [#id]` |

---

## Event Format

Events are emitted by the Arduino:

```
EVENT_TYPE [action] [position] [#command_id]
```

### Event Types

| Event | Format | Description |
|-------|--------|-------------|
| `ACK` | `ACK <action> <pos> [#id]` | Command acknowledged |
| `DONE` | `DONE <action> <pos> [#id]` | Long-running command completed |
| `TOUCHED_DOWN` | `TOUCHED_DOWN <pos> [#id]` | Touch detected (after EXPECT_DOWN) |
| `TOUCHED_UP` | `TOUCHED_UP <pos> [#id]` | Release detected (after EXPECT_UP) |
| `ERR` | `ERR <reason> [#id]` | Error occurred |

### Error Reasons
- `bad_format` - Malformed command
- `unknown_action` - Unknown command
- `unknown_position` - Invalid position letter
- `command_failed` - Hardware operation failed
- `busy` - Command queue full
- `no_touch_controller` - Touch hardware not available

---

## Timing Characteristics

| Operation | Timing |
|-----------|--------|
| SHOW/HIDE | Instant (~1ms) |
| SUCCESS animation | ~400ms (5 expansion steps × 80ms) |
| BLINK rate | 150ms on/off cycle |
| Touch debounce | 30ms |
| Touch poll interval | 10ms |
| SEQUENCE_COMPLETED | ~1200ms |

---

## Command ID Best Practices

Using command IDs enables:
1. **Correlation** - Match responses to requests
2. **Async handling** - Track multiple pending commands
3. **Debugging** - Trace command flow

**Recommendation:** Use incrementing IDs starting from 1000.

```python
class CommandTracker:
    def __init__(self):
        self._next_id = 1000
    
    def next_id(self) -> int:
        id = self._next_id
        self._next_id += 1
        return id
```

---

## State Patterns

### Pattern 1: Simple Touch Sequence
Show LED → Wait for touch → Success animation → Hide

```python
async def simple_step(self, position: str):
    await self.send(f"SHOW {position}")
    await self.send(f"EXPECT_DOWN {position}")
    await self.wait_for_event("TOUCHED_DOWN", position)
    await self.send(f"SUCCESS {position}")
    await self.wait_for_event("DONE", "SUCCESS", position)
    await self.send(f"HIDE {position}")
```

### Pattern 2: Two-Hand Overlapping Sequence
User must hold one position while touching the next, then release the first.

```
Position A: SHOW → touch → SUCCESS
Position B: SHOW → touch → SUCCESS → BLINK A
Wait: release A → STOP_BLINK A → HIDE A → SHOW C
Position C: touch → SUCCESS → BLINK B
Wait: release B → STOP_BLINK B → HIDE B → SHOW D
Position D: touch → SEQUENCE_COMPLETED
```

### Pattern 3: Simultaneous Touch
Multiple positions must be touched within a time window.

```python
async def simultaneous_step(self, positions: list[str], window_ms: int = 500):
    for pos in positions:
        await self.send(f"SHOW {pos}")
        await self.send(f"EXPECT_DOWN {pos}")
    
    touched = set()
    start_time = time.time()
    
    while len(touched) < len(positions):
        if (time.time() - start_time) * 1000 > window_ms:
            raise TimeoutError("Simultaneous touch window expired")
        
        event = await self.wait_for_any_event("TOUCHED_DOWN")
        touched.add(event.position)
    
    # All touched within window - success!
    for pos in positions:
        await self.send(f"SUCCESS {pos}")
```

---

## Python Implementation Guide

### Recommended Architecture

```
pi_controller/
├── arduino_interface.py    # Low-level serial communication
├── sequence_controller.py  # Base class with high-level methods
├── programs/
│   ├── __init__.py
│   ├── simple_sequence.py
│   ├── two_hand_sequence.py
│   └── simultaneous_sequence.py
└── main.py                 # Program selection and startup
```

### arduino_interface.py

Handles serial communication, command ID tracking, and event parsing.

```python
import serial
import asyncio
from dataclasses import dataclass
from typing import Optional, Callable
import threading
import queue

@dataclass
class ArduinoEvent:
    event_type: str      # ACK, DONE, TOUCHED_DOWN, TOUCHED_UP, ERR, etc.
    action: Optional[str] = None
    position: Optional[str] = None
    command_id: Optional[int] = None
    raw_line: str = ""

class ArduinoInterface:
    def __init__(self, port: str = "/dev/ttyACM0", baud: int = 115200):
        self.port = port
        self.baud = baud
        self.serial: Optional[serial.Serial] = None
        self._next_command_id = 1000
        self._event_queue = queue.Queue()
        self._reader_thread: Optional[threading.Thread] = None
        self._running = False
        self._event_callbacks: list[Callable[[ArduinoEvent], None]] = []
    
    def connect(self):
        """Open serial connection and start reader thread."""
        self.serial = serial.Serial(self.port, self.baud, timeout=0.1)
        self._running = True
        self._reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader_thread.start()
    
    def disconnect(self):
        """Close serial connection."""
        self._running = False
        if self._reader_thread:
            self._reader_thread.join(timeout=1.0)
        if self.serial:
            self.serial.close()
    
    def _reader_loop(self):
        """Background thread that reads and parses serial data."""
        while self._running:
            try:
                if self.serial and self.serial.in_waiting:
                    line = self.serial.readline().decode('utf-8').strip()
                    if line:
                        event = self._parse_event(line)
                        if event:
                            self._event_queue.put(event)
                            for callback in self._event_callbacks:
                                callback(event)
            except Exception as e:
                print(f"Reader error: {e}")
    
    def _parse_event(self, line: str) -> Optional[ArduinoEvent]:
        """Parse an event line from Arduino."""
        # Skip echo lines (commands we sent)
        if line.startswith("PI>"):
            return None
        
        parts = line.split()
        if not parts:
            return None
        
        event = ArduinoEvent(event_type=parts[0], raw_line=line)
        
        # Parse command ID if present
        for part in parts:
            if part.startswith("#"):
                try:
                    event.command_id = int(part[1:])
                except ValueError:
                    pass
        
        # Parse based on event type
        if event.event_type in ("TOUCHED_DOWN", "TOUCHED_UP"):
            if len(parts) >= 2 and parts[1].isalpha() and len(parts[1]) == 1:
                event.position = parts[1].upper()
        
        elif event.event_type in ("ACK", "DONE"):
            if len(parts) >= 2:
                event.action = parts[1]
            if len(parts) >= 3 and parts[2].isalpha() and len(parts[2]) == 1:
                event.position = parts[2].upper()
        
        elif event.event_type == "ERR":
            if len(parts) >= 2:
                event.action = parts[1]  # Error reason
        
        return event
    
    def send_command(self, command: str, use_id: bool = True) -> int:
        """Send a command to Arduino. Returns command ID."""
        cmd_id = None
        if use_id:
            cmd_id = self._next_command_id
            self._next_command_id += 1
            command = f"{command} #{cmd_id}"
        
        if self.serial:
            self.serial.write(f"{command}\n".encode('utf-8'))
        
        return cmd_id
    
    def get_event(self, timeout: float = None) -> Optional[ArduinoEvent]:
        """Get next event from queue."""
        try:
            return self._event_queue.get(timeout=timeout)
        except queue.Empty:
            return None
    
    def add_event_callback(self, callback: Callable[[ArduinoEvent], None]):
        """Register callback for all events."""
        self._event_callbacks.append(callback)
```

### sequence_controller.py

Base class providing high-level methods for sequence programs.

```python
import asyncio
from typing import Optional
from arduino_interface import ArduinoInterface, ArduinoEvent

class SequenceController:
    """
    Base class for sequence programs.
    
    Provides high-level methods for controlling LEDs and touch sensors.
    Subclass this to create specific sequence programs.
    """
    
    def __init__(self, arduino: ArduinoInterface):
        self.arduino = arduino
        self._pending_events: dict[int, asyncio.Future] = {}
        self._touch_events: asyncio.Queue = asyncio.Queue()
        
        # Register event handler
        arduino.add_event_callback(self._on_event)
    
    def _on_event(self, event: ArduinoEvent):
        """Handle incoming events."""
        # Resolve pending command futures
        if event.command_id and event.command_id in self._pending_events:
            future = self._pending_events.pop(event.command_id)
            if not future.done():
                future.set_result(event)
        
        # Queue touch events
        if event.event_type in ("TOUCHED_DOWN", "TOUCHED_UP"):
            asyncio.get_event_loop().call_soon_threadsafe(
                self._touch_events.put_nowait, event
            )
    
    # =========================================================================
    # Low-Level Commands
    # =========================================================================
    
    async def send(self, command: str) -> ArduinoEvent:
        """Send command and wait for ACK."""
        cmd_id = self.arduino.send_command(command)
        return await self._wait_for_command_response(cmd_id, timeout=1.0)
    
    async def _wait_for_command_response(self, cmd_id: int, timeout: float) -> ArduinoEvent:
        """Wait for ACK or ERR for a specific command ID."""
        future = asyncio.get_event_loop().create_future()
        self._pending_events[cmd_id] = future
        try:
            return await asyncio.wait_for(future, timeout=timeout)
        except asyncio.TimeoutError:
            self._pending_events.pop(cmd_id, None)
            raise TimeoutError(f"No response for command #{cmd_id}")
    
    # =========================================================================
    # LED Control Methods
    # =========================================================================
    
    async def show(self, position: str):
        """Show LED at position (blue)."""
        await self.send(f"SHOW {position}")
    
    async def hide(self, position: str):
        """Hide LED at position."""
        await self.send(f"HIDE {position}")
    
    async def success(self, position: str, wait_for_done: bool = True):
        """
        Play success animation at position (green expansion).
        
        Args:
            position: Position letter (A-Y)
            wait_for_done: If True, wait for animation to complete
        """
        event = await self.send(f"SUCCESS {position}")
        if wait_for_done:
            await self.wait_for_done("SUCCESS", position)
    
    async def blink(self, position: str):
        """
        Start blinking LED at position (orange, fast).
        Use this to signal "release this position".
        """
        await self.send(f"BLINK {position}")
    
    async def stop_blink(self, position: str):
        """Stop blinking and turn off LED."""
        await self.send(f"STOP_BLINK {position}")
    
    async def sequence_completed(self, wait_for_done: bool = True):
        """Play celebration animation on all LEDs."""
        await self.send("SEQUENCE_COMPLETED")
        if wait_for_done:
            await self.wait_for_done("SEQUENCE_COMPLETED")
    
    # =========================================================================
    # Touch Control Methods
    # =========================================================================
    
    async def expect_touch(self, position: str):
        """Register to receive TOUCHED_DOWN event for position."""
        await self.send(f"EXPECT_DOWN {position}")
    
    async def expect_release(self, position: str):
        """Register to receive TOUCHED_UP event for position."""
        await self.send(f"EXPECT_UP {position}")
    
    async def wait_for_touch(self, position: str, timeout: float = None) -> ArduinoEvent:
        """
        Wait for touch at specific position.
        Must call expect_touch() first!
        """
        return await self._wait_for_touch_event("TOUCHED_DOWN", position, timeout)
    
    async def wait_for_release(self, position: str, timeout: float = None) -> ArduinoEvent:
        """
        Wait for release at specific position.
        Must call expect_release() first!
        """
        return await self._wait_for_touch_event("TOUCHED_UP", position, timeout)
    
    async def _wait_for_touch_event(
        self, 
        event_type: str, 
        position: str, 
        timeout: float = None
    ) -> ArduinoEvent:
        """Wait for a specific touch event."""
        while True:
            try:
                event = await asyncio.wait_for(
                    self._touch_events.get(), 
                    timeout=timeout
                )
                if event.event_type == event_type and event.position == position.upper():
                    return event
                # Re-queue events for other positions
                # (or discard if not needed)
            except asyncio.TimeoutError:
                raise TimeoutError(f"Timeout waiting for {event_type} at {position}")
    
    async def wait_for_any_touch(self, timeout: float = None) -> ArduinoEvent:
        """Wait for any touch event."""
        return await asyncio.wait_for(self._touch_events.get(), timeout=timeout)
    
    # =========================================================================
    # Utility Methods
    # =========================================================================
    
    async def wait_for_done(self, action: str, position: str = None, timeout: float = 5.0):
        """Wait for DONE event for a long-running command."""
        # Implementation depends on your event handling strategy
        # This is a simplified version
        start = asyncio.get_event_loop().time()
        while True:
            event = self.arduino.get_event(timeout=0.1)
            if event and event.event_type == "DONE" and event.action == action:
                if position is None or event.position == position.upper():
                    return event
            if asyncio.get_event_loop().time() - start > timeout:
                raise TimeoutError(f"Timeout waiting for DONE {action}")
    
    async def ping(self) -> bool:
        """Check if Arduino is responsive."""
        try:
            await self.send("PING")
            return True
        except TimeoutError:
            return False
    
    async def recalibrate_all(self):
        """Recalibrate all touch sensors."""
        await self.send("RECALIBRATE_ALL")
    
    # =========================================================================
    # High-Level Sequence Patterns
    # =========================================================================
    
    async def simple_step(self, position: str):
        """
        Execute a simple step: show → touch → success → hide.
        
        Args:
            position: Position letter (A-Y)
        """
        await self.show(position)
        await self.expect_touch(position)
        await self.wait_for_touch(position)
        await self.success(position)
        await self.hide(position)
    
    async def show_and_wait_touch(self, position: str, timeout: float = None) -> bool:
        """
        Show LED and wait for touch.
        
        Returns:
            True if touched, False if timeout
        """
        await self.show(position)
        await self.expect_touch(position)
        try:
            await self.wait_for_touch(position, timeout=timeout)
            return True
        except TimeoutError:
            return False
    
    async def blink_and_wait_release(self, position: str, timeout: float = None):
        """
        Start blinking position and wait for release.
        Automatically stops blink and hides when released.
        """
        await self.blink(position)
        await self.expect_release(position)
        await self.wait_for_release(position, timeout=timeout)
        await self.stop_blink(position)
        await self.hide(position)
    
    # =========================================================================
    # Abstract Methods (Override in Subclass)
    # =========================================================================
    
    async def run(self):
        """
        Main entry point for the sequence program.
        Override this in your subclass.
        """
        raise NotImplementedError("Subclass must implement run()")
    
    def get_positions(self) -> list[str]:
        """
        Return the list of positions used in this sequence.
        Override if your sequence uses specific positions.
        """
        return list("ABCDEFGHIJKLMNOPQRSTUVWXY")
```

### Example Program: two_hand_sequence.py

```python
from sequence_controller import SequenceController

class TwoHandSequence(SequenceController):
    """
    Two-hand overlapping sequence.
    
    User must hold position N-1 while touching position N,
    then release N-1 when prompted (blinking).
    
    Pattern for positions A, B, C, D:
    1. Show A → touch A → success A
    2. Show B → touch B (while holding A) → success B → blink A
    3. Wait release A → hide A → show C
    4. Touch C (while holding B) → success C → blink B
    5. Wait release B → hide B → show D
    6. Touch D → sequence completed!
    """
    
    def __init__(self, arduino, positions: str = "ABCD"):
        super().__init__(arduino)
        self.positions = list(positions.upper())
    
    async def run(self):
        """Execute the two-hand sequence."""
        
        if len(self.positions) < 2:
            raise ValueError("Two-hand sequence requires at least 2 positions")
        
        print(f"Starting two-hand sequence: {' → '.join(self.positions)}")
        
        # === First position: simple touch ===
        pos_a = self.positions[0]
        await self.show(pos_a)
        await self.expect_touch(pos_a)
        print(f"Touch {pos_a}...")
        await self.wait_for_touch(pos_a)
        await self.success(pos_a)
        print(f"✓ {pos_a} touched!")
        
        # === Second position: touch while holding first ===
        pos_b = self.positions[1]
        await self.show(pos_b)
        await self.expect_touch(pos_b)
        print(f"Keep holding {pos_a}, now touch {pos_b}...")
        await self.wait_for_touch(pos_b)
        await self.success(pos_b)
        print(f"✓ {pos_b} touched!")
        
        # === Remaining positions ===
        for i in range(2, len(self.positions)):
            pos_current = self.positions[i]
            pos_to_release = self.positions[i - 1]
            pos_blinking = self.positions[i - 1]
            
            # Start blinking the position to release
            await self.blink(pos_blinking)
            await self.expect_release(pos_blinking)
            print(f"Release {pos_blinking} (blinking)...")
            
            # Wait for release
            await self.wait_for_release(pos_blinking)
            await self.stop_blink(pos_blinking)
            await self.hide(pos_blinking)
            print(f"✓ {pos_blinking} released!")
            
            # Show next position
            await self.show(pos_current)
            await self.expect_touch(pos_current)
            print(f"Now touch {pos_current}...")
            
            # Wait for touch
            await self.wait_for_touch(pos_current)
            await self.success(pos_current)
            print(f"✓ {pos_current} touched!")
        
        # === Final cleanup: release last held position ===
        if len(self.positions) >= 2:
            pos_last_held = self.positions[-2]
            await self.blink(pos_last_held)
            await self.expect_release(pos_last_held)
            print(f"Release {pos_last_held} to complete...")
            await self.wait_for_release(pos_last_held)
            await self.stop_blink(pos_last_held)
            await self.hide(pos_last_held)
        
        # === Celebration! ===
        print("🎉 Sequence completed!")
        await self.sequence_completed()


# Usage
if __name__ == "__main__":
    import asyncio
    from arduino_interface import ArduinoInterface
    
    async def main():
        arduino = ArduinoInterface(port="/dev/ttyACM0")
        arduino.connect()
        
        try:
            sequence = TwoHandSequence(arduino, positions="ABCD")
            await sequence.run()
        finally:
            arduino.disconnect()
    
    asyncio.run(main())
```

### Example Program: simple_sequence.py

```python
from sequence_controller import SequenceController

class SimpleSequence(SequenceController):
    """
    Simple sequential touch program.
    
    User touches each position one at a time.
    Each step: show → touch → success → hide → next
    """
    
    def __init__(self, arduino, positions: str = "ABCDE"):
        super().__init__(arduino)
        self.positions = list(positions.upper())
    
    async def run(self):
        """Execute the simple sequence."""
        
        print(f"Starting simple sequence: {' → '.join(self.positions)}")
        
        for i, position in enumerate(self.positions):
            print(f"Step {i+1}/{len(self.positions)}: Touch {position}")
            
            # Show LED
            await self.show(position)
            
            # Wait for touch
            await self.expect_touch(position)
            await self.wait_for_touch(position)
            
            # Success animation
            await self.success(position)
            print(f"✓ {position} complete!")
            
            # Wait for release before hiding
            await self.expect_release(position)
            await self.wait_for_release(position)
            
            # Hide LED
            await self.hide(position)
        
        # Celebration
        print("🎉 Sequence completed!")
        await self.sequence_completed()
```

### Example Program: simultaneous_sequence.py

```python
from sequence_controller import SequenceController
import asyncio

class SimultaneousSequence(SequenceController):
    """
    Sequence with simultaneous touch requirements.
    
    Some steps require multiple positions to be touched
    within a time window.
    
    Step format: "A" for single, "(A+B)" for simultaneous
    Example: "A,(B+C),D" - touch A, then B and C together, then D
    """
    
    def __init__(self, arduino, spec: str = "A,(B+C),D"):
        super().__init__(arduino)
        self.steps = self._parse_spec(spec)
    
    def _parse_spec(self, spec: str) -> list[list[str]]:
        """Parse step specification."""
        steps = []
        current_group = []
        in_group = False
        
        for char in spec.upper():
            if char == '(':
                in_group = True
                current_group = []
            elif char == ')':
                in_group = False
                if current_group:
                    steps.append(current_group)
                current_group = []
            elif char.isalpha() and 'A' <= char <= 'Y':
                if in_group:
                    current_group.append(char)
                else:
                    steps.append([char])
            # Ignore commas, +, spaces
        
        return steps
    
    async def run(self, window_ms: int = 500):
        """Execute the simultaneous sequence."""
        
        print(f"Starting simultaneous sequence: {self.steps}")
        
        for i, positions in enumerate(self.steps):
            if len(positions) == 1:
                # Single position
                await self._single_step(positions[0], i + 1)
            else:
                # Simultaneous positions
                await self._simultaneous_step(positions, i + 1, window_ms)
        
        print("🎉 Sequence completed!")
        await self.sequence_completed()
    
    async def _single_step(self, position: str, step_num: int):
        """Handle single position step."""
        print(f"Step {step_num}: Touch {position}")
        
        await self.show(position)
        await self.expect_touch(position)
        await self.wait_for_touch(position)
        await self.success(position)
        
        await self.expect_release(position)
        await self.wait_for_release(position)
        await self.hide(position)
        
        print(f"✓ {position} complete!")
    
    async def _simultaneous_step(self, positions: list[str], step_num: int, window_ms: int):
        """Handle simultaneous touch step."""
        positions_str = "+".join(positions)
        print(f"Step {step_num}: Touch {positions_str} simultaneously!")
        
        # Show all LEDs
        for pos in positions:
            await self.show(pos)
            await self.expect_touch(pos)
        
        # Wait for all touches within window
        touched = set()
        start_time = asyncio.get_event_loop().time()
        
        while len(touched) < len(positions):
            elapsed_ms = (asyncio.get_event_loop().time() - start_time) * 1000
            remaining_ms = window_ms - elapsed_ms
            
            if remaining_ms <= 0:
                # Timeout - reset and retry
                print(f"⚠ Too slow! Touch all positions within {window_ms}ms")
                touched.clear()
                start_time = asyncio.get_event_loop().time()
                continue
            
            try:
                event = await self.wait_for_any_touch(timeout=remaining_ms / 1000)
                if event.position in positions:
                    touched.add(event.position)
                    print(f"  ✓ {event.position} touched ({len(touched)}/{len(positions)})")
            except TimeoutError:
                continue
        
        # All touched! Play success on all
        for pos in positions:
            await self.success(pos, wait_for_done=False)
        
        # Wait for all releases
        for pos in positions:
            await self.expect_release(pos)
        
        for pos in positions:
            await self.wait_for_release(pos)
            await self.hide(pos)
        
        print(f"✓ {positions_str} complete!")
```

---

## Debugging Tips

### Serial Monitor
Connect to Arduino serial at 115200 baud to see all commands and events:
```bash
screen /dev/ttyACM0 115200
```

### Common Issues

| Symptom | Cause | Solution |
|---------|-------|----------|
| No ACK received | Serial not connected | Check port, baud rate |
| ERR busy | Command queue full | Wait for pending commands |
| Touch not detected | Sensor not calibrated | Send RECALIBRATE_ALL |
| Wrong position responds | Wiring issue | Check sensor mapping |

### Test Commands
Send these manually to verify hardware:
```
PING
INFO
SCAN
SHOW A
HIDE A
BLINK B
STOP_BLINK B
```

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────┐
│                    COMMAND QUICK REFERENCE                       │
├─────────────────────────────────────────────────────────────────┤
│ LED Control:                                                     │
│   SHOW <pos>        → Light LED (blue)                          │
│   HIDE <pos>        → Turn off LED                              │
│   SUCCESS <pos>     → Green expansion animation                 │
│   BLINK <pos>       → Fast orange blink ("release me!")         │
│   STOP_BLINK <pos>  → Stop blinking                             │
│   SEQUENCE_COMPLETED → Celebration animation                     │
├─────────────────────────────────────────────────────────────────┤
│ Touch Control:                                                   │
│   EXPECT_DOWN <pos> → Arm touch detection                       │
│   EXPECT_UP <pos>   → Arm release detection                     │
├─────────────────────────────────────────────────────────────────┤
│ Events from Arduino:                                             │
│   ACK <cmd> <pos>   → Command acknowledged                      │
│   DONE <cmd> <pos>  → Animation complete                        │
│   TOUCHED_DOWN <pos>→ Touch detected                            │
│   TOUCHED_UP <pos>  → Release detected                          │
│   ERR <reason>      → Error occurred                            │
├─────────────────────────────────────────────────────────────────┤
│ Positions: A B C D E F G H I J K L M N O P Q R S T U V W X Y   │
│ Baud: 115200 | Line ending: \n | IDs: #1000, #1001, ...        │
└─────────────────────────────────────────────────────────────────┘
```
