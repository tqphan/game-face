"""
Input controller backed by evdev / UInput (Linux only).

Port of a pynput-based controller. Instead of a high-level API it creates
virtual input devices with `evdev.UInput` and injects raw kernel input
events, using evdev's own codes exclusively (KEY_*, BTN_*, REL_*, ABS_*).

Requirements / caveats:
  * Linux only. Needs write access to /dev/uinput (usually root, or a udev
    rule granting your user access to the `uinput` group).
  * keyboard.type() maps characters using a US QWERTY layout.
  * Absolute positioning (mouse.position) is done through a separate virtual
    *absolute* pointer whose range is mapped to the screen resolution passed
    to the constructor. Whether it lands on the exact pixel depends on the
    compositor (X11/Wayland) treating it like a tablet/touch device, so tune
    `screen_width`/`screen_height` to your display.
"""

import re
import time

from evdev import UInput, AbsInfo, ecodes as e


MOUSE_BUTTONS = [e.BTN_LEFT, e.BTN_RIGHT, e.BTN_MIDDLE, e.BTN_SIDE, e.BTN_EXTRA]


def _build_char_map():
    """char -> (keycode, needs_shift) using a US QWERTY layout (for type())."""
    m = {}
    for c in 'abcdefghijklmnopqrstuvwxyz':
        code = getattr(e, f'KEY_{c.upper()}')
        m[c] = (code, False)
        m[c.upper()] = (code, True)

    digit_shift = {
        '1': '!', '2': '@', '3': '#', '4': '$', '5': '%',
        '6': '^', '7': '&', '8': '*', '9': '(', '0': ')',
    }
    for d, sym in digit_shift.items():
        code = getattr(e, f'KEY_{d}')
        m[d] = (code, False)
        m[sym] = (code, True)

    m.update({
        '-':  (e.KEY_MINUS, False),      '_': (e.KEY_MINUS, True),
        '=':  (e.KEY_EQUAL, False),      '+': (e.KEY_EQUAL, True),
        '[':  (e.KEY_LEFTBRACE, False),  '{': (e.KEY_LEFTBRACE, True),
        ']':  (e.KEY_RIGHTBRACE, False), '}': (e.KEY_RIGHTBRACE, True),
        '\\': (e.KEY_BACKSLASH, False),  '|': (e.KEY_BACKSLASH, True),
        ';':  (e.KEY_SEMICOLON, False),  ':': (e.KEY_SEMICOLON, True),
        "'":  (e.KEY_APOSTROPHE, False), '"': (e.KEY_APOSTROPHE, True),
        '`':  (e.KEY_GRAVE, False),      '~': (e.KEY_GRAVE, True),
        ',':  (e.KEY_COMMA, False),      '<': (e.KEY_COMMA, True),
        '.':  (e.KEY_DOT, False),        '>': (e.KEY_DOT, True),
        '/':  (e.KEY_SLASH, False),      '?': (e.KEY_SLASH, True),
        ' ':  (e.KEY_SPACE, False),
        '\t': (e.KEY_TAB, False),
        '\n': (e.KEY_ENTER, False),
    })
    return m


CHAR_MAP = _build_char_map()


class Controller:
    def __init__(self, screen_width=1920, screen_height=1080,
                 click_interval=0.01, settle_time=0.2):
        self.click_interval = click_interval

        # ---- relative pointer + keyboard device -----------------------
        rel_caps = {
            e.EV_KEY: list(range(1, 249)) + MOUSE_BUTTONS,
            e.EV_REL: [e.REL_X, e.REL_Y, e.REL_WHEEL, e.REL_HWHEEL],
        }
        self.ui = UInput(rel_caps, name='game-face-controller')

        # ---- absolute pointer device ----------------------------------
        abs_caps = {
            e.EV_KEY: MOUSE_BUTTONS,
            e.EV_ABS: [
                (e.ABS_X, AbsInfo(value=0, min=0, max=max(1, screen_width - 1),
                                  fuzz=0, flat=0, resolution=0)),
                (e.ABS_Y, AbsInfo(value=0, min=0, max=max(1, screen_height - 1),
                                  fuzz=0, flat=0, resolution=0)),
            ],
        }
        self.abs_ui = UInput(abs_caps, name='game-face-controller-abs')

        # Give the kernel/compositor a moment to register the new devices.
        time.sleep(settle_time)

        self.patterns = {
            'function_call': re.compile(r'^([\w.]+)\((.*)\)$'),
            'tuple': re.compile(r'^\((-?\d+),\s*(-?\d+)\)$'),
            'evkey': re.compile(r'^KEY_\w+$'),
            'evbtn': re.compile(r'^BTN_\w+$'),
        }

        self.allowed_methods = {
            'keyboard.press':   self._keyboard_press,
            'keyboard.release': self._keyboard_release,
            'keyboard.type':    self._keyboard_type,
            'mouse.move':       self._mouse_move,
            'mouse.click':      self._mouse_click,
            'mouse.scroll':     self._mouse_scroll,
            'mouse.position':   self._mouse_position,
            'mouse.press':      self._mouse_press,
            'mouse.release':    self._mouse_release,
        }

    def close(self):
        for dev in (getattr(self, 'ui', None), getattr(self, 'abs_ui', None)):
            if dev is not None:
                try:
                    dev.close()
                except Exception:
                    pass

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # ------------------------------------------------------------------
    # Low-level emit helper
    # ------------------------------------------------------------------

    def _emit_key(self, code, value):
        self.ui.write(e.EV_KEY, code, value)
        self.ui.syn()

    # ------------------------------------------------------------------
    # Keyboard handlers
    # ------------------------------------------------------------------

    def _keyboard_press(self, args):
        key = self._parse_key(args[0])
        if key is not None:
            self._emit_key(key, 1)
            return True
        return False

    def _keyboard_release(self, args):
        key = self._parse_key(args[0])
        if key is not None:
            self._emit_key(key, 0)
            return True
        return False

    def _keyboard_type(self, args):
        text = self._parse_string(args[0])
        if text is None:
            return False
        ok = True
        for ch in text:
            if not self._type_char(ch):
                print(f"Cannot type character: {ch!r}")
                ok = False
        return ok

    def _type_char(self, ch):
        entry = CHAR_MAP.get(ch)
        if entry is None:
            return False
        code, shift = entry
        if shift:
            self._emit_key(e.KEY_LEFTSHIFT, 1)
        self._emit_key(code, 1)
        self._emit_key(code, 0)
        if shift:
            self._emit_key(e.KEY_LEFTSHIFT, 0)
        return True

    # ------------------------------------------------------------------
    # Mouse handlers
    # ------------------------------------------------------------------

    def _mouse_move(self, args):
        if len(args) != 2:
            return False
        try:
            dx, dy = int(args[0]), int(args[1])
        except ValueError:
            return False
        self.ui.write(e.EV_REL, e.REL_X, dx)
        self.ui.write(e.EV_REL, e.REL_Y, dy)
        self.ui.syn()
        return True

    def _mouse_position(self, args):
        if len(args) != 2:
            return False
        try:
            x, y = int(args[0]), int(args[1])
        except ValueError:
            return False
        self.abs_ui.write(e.EV_ABS, e.ABS_X, x)
        self.abs_ui.write(e.EV_ABS, e.ABS_Y, y)
        self.abs_ui.syn()
        return True

    def _mouse_click(self, args):
        if len(args) < 1 or len(args) > 2:
            return False
        button = self._parse_button(args[0])
        if button is None:
            return False
        count = 1
        if len(args) == 2:
            try:
                count = int(args[1])
            except ValueError:
                return False
        for i in range(count):
            self._emit_key(button, 1)
            self._emit_key(button, 0)
            if i < count - 1:
                time.sleep(self.click_interval)
        return True

    def _mouse_scroll(self, args):
        if len(args) != 2:
            return False
        try:
            dx, dy = int(args[0]), int(args[1])
        except ValueError:
            return False
        if dx:
            self.ui.write(e.EV_REL, e.REL_HWHEEL, dx)
        if dy:
            self.ui.write(e.EV_REL, e.REL_WHEEL, dy)
        self.ui.syn()
        return True

    def _mouse_press(self, args):
        if len(args) != 1:
            return False
        button = self._parse_button(args[0])
        if button is None:
            return False
        self._emit_key(button, 1)
        return True

    def _mouse_release(self, args):
        if len(args) != 1:
            return False
        button = self._parse_button(args[0])
        if button is None:
            return False
        self._emit_key(button, 0)
        return True

    # ------------------------------------------------------------------
    # Parsers
    # ------------------------------------------------------------------

    def _parse_key(self, key_str):
        key_str = key_str.strip()
        if self.patterns['evkey'].match(key_str):
            code = e.ecodes.get(key_str)
            if isinstance(code, int):
                return code
            print(f"Unknown key: {key_str}")
            return None
        print(f"Invalid key format: {key_str}")
        return None

    def _parse_button(self, button_str):
        button_str = button_str.strip()
        if self.patterns['evbtn'].match(button_str):
            code = e.ecodes.get(button_str)
            if isinstance(code, int):
                return code
            print(f"Unknown button: {button_str}")
            return None
        print(f"Invalid button format: {button_str}")
        return None

    def _parse_string(self, string_str):
        string_str = string_str.strip()
        if (string_str.startswith("'") and string_str.endswith("'")) or \
           (string_str.startswith('"') and string_str.endswith('"')):
            return string_str[1:-1]
        return None

    def _parse_arguments(self, arg_str):
        if not arg_str.strip():
            return []

        args = []
        current_arg = ""
        paren_depth = 0
        in_quotes = False
        quote_char = None

        for char in arg_str:
            if char in ('"', "'") and not in_quotes:
                in_quotes = True
                quote_char = char
                current_arg += char
            elif char == quote_char and in_quotes:
                in_quotes = False
                quote_char = None
                current_arg += char
            elif char == '(' and not in_quotes:
                paren_depth += 1
                current_arg += char
            elif char == ')' and not in_quotes:
                paren_depth -= 1
                current_arg += char
            elif char == ',' and paren_depth == 0 and not in_quotes:
                args.append(current_arg.strip())
                current_arg = ""
            else:
                current_arg += char

        if current_arg.strip():
            args.append(current_arg.strip())

        return args

    # ------------------------------------------------------------------
    # Command dispatch
    # ------------------------------------------------------------------

    def execute_command(self, command_str):
        command_str = command_str.strip()
        if '=' in command_str and not self.patterns['function_call'].match(command_str):
            return self._execute_assignment(command_str)
        return self._execute_function_call(command_str)

    def _execute_assignment(self, command_str):
        parts = command_str.split('=', 1)
        if len(parts) != 2:
            print(f"Invalid assignment: {command_str}")
            return False

        var_name = parts[0].strip()
        value = parts[1].strip()

        if var_name != 'mouse.position':
            print(f"Unknown variable: {var_name}")
            return False

        match = self.patterns['tuple'].match(value)
        if not match:
            print(f"Invalid tuple format: {value}")
            return False

        return self._mouse_position([match.group(1), match.group(2)])

    def _execute_function_call(self, command_str):
        match = self.patterns['function_call'].match(command_str)
        if not match:
            print(f"Invalid command format: {command_str}")
            return False

        method_name = match.group(1)
        arg_str = match.group(2)

        if method_name not in self.allowed_methods:
            print(f"Method '{method_name}' not allowed")
            return False

        args = self._parse_arguments(arg_str)

        try:
            return self.allowed_methods[method_name](args)
        except Exception as ex:
            print(f"Error executing {command_str}: {ex}")
            return False