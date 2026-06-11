import re
from pynput.keyboard import Key, KeyCode, Controller as KeyboardController
from pynput.mouse import Button, Controller as MouseController

NUMPAD_KEYS = {
    'numpad_0': KeyCode.from_vk(0xFF9E),
    'numpad_1': KeyCode.from_vk(0xFF9C),
    'numpad_2': KeyCode.from_vk(0xFF99),
    'numpad_3': KeyCode.from_vk(0xFF9B),
    'numpad_4': KeyCode.from_vk(0xFF96),
    'numpad_5': KeyCode.from_vk(0xFF9D),
    'numpad_6': KeyCode.from_vk(0xFF98),
    'numpad_7': KeyCode.from_vk(0xFF95),
    'numpad_8': KeyCode.from_vk(0xFF97),
    'numpad_9': KeyCode.from_vk(0xFF9A),
    'numpad_add':     KeyCode.from_vk(0xFF2B),
    'numpad_sub':     KeyCode.from_vk(0xFFAD),
    'numpad_mul':     KeyCode.from_vk(0xFFAA),
    'numpad_div':     KeyCode.from_vk(0xFFAF),
    'numpad_decimal': KeyCode.from_vk(0xFFAE),
    'numpad_enter':   KeyCode.from_vk(0xFF8D),
}


class Controller:
    def __init__(self):
        self.keyboard = KeyboardController()
        self.mouse = MouseController()

        self.patterns = {
            'function_call': re.compile(r'^([\w.]+)\((.*)\)$'),
            'tuple': re.compile(r'^\((-?\d+),\s*(-?\d+)\)$'),  # fixed: allow negative
            'button': re.compile(r'^Button\.(\w+)$'),
            'key': re.compile(r'^Key\.(\w+)$'),
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

    # ------------------------------------------------------------------
    # Keyboard handlers
    # ------------------------------------------------------------------

    def _keyboard_press(self, args):
        key = self._parse_key(args[0])
        if key is not None:
            self.keyboard.press(key)
            return True
        return False

    def _keyboard_release(self, args):
        key = self._parse_key(args[0])
        if key is not None:
            self.keyboard.release(key)
            return True
        return False

    def _keyboard_type(self, args):
        text = self._parse_string(args[0])
        if text is not None:
            self.keyboard.type(text)
            return True
        return False

    # ------------------------------------------------------------------
    # Mouse handlers
    # ------------------------------------------------------------------

    def _mouse_move(self, args):
        if len(args) != 2:
            return False
        try:
            dx, dy = int(args[0]), int(args[1])
            self.mouse.move(dx, dy)
            return True
        except ValueError:
            return False

    def _mouse_position(self, args):
        if len(args) != 2:
            return False
        try:
            x, y = int(args[0]), int(args[1])
            self.mouse.position = (x, y)
            return True
        except ValueError:
            return False

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
        self.mouse.click(button, count)
        return True

    def _mouse_scroll(self, args):
        if len(args) != 2:
            return False
        try:
            dx, dy = int(args[0]), int(args[1])
            self.mouse.scroll(dx, dy)
            return True
        except ValueError:
            return False

    def _mouse_press(self, args):
        if len(args) != 1:
            return False
        button = self._parse_button(args[0])
        if button is None:
            return False
        self.mouse.press(button)
        return True

    def _mouse_release(self, args):
        if len(args) != 1:
            return False
        button = self._parse_button(args[0])
        if button is None:
            return False
        self.mouse.release(button)
        return True

    # ------------------------------------------------------------------
    # Parsers
    # ------------------------------------------------------------------

    def _parse_key(self, key_str):
        key_str = key_str.strip()

        # Numpad lookup
        if key_str in NUMPAD_KEYS:
            return NUMPAD_KEYS[key_str]

        # Key.* constants
        match = self.patterns['key'].match(key_str)
        if match:
            key_name = match.group(1)
            if hasattr(Key, key_name):
                return getattr(Key, key_name)
            print(f"Unknown key: {key_name}")
            return None

        # Single quoted character
        string = self._parse_string(key_str)
        if string is not None and len(string) == 1:
            return string

        print(f"Invalid key format: {key_str}")
        return None

    def _parse_button(self, button_str):
        button_str = button_str.strip()
        match = self.patterns['button'].match(button_str)
        if match:
            button_name = match.group(1)
            if hasattr(Button, button_name):
                return getattr(Button, button_name)
            print(f"Unknown button: {button_name}")
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
        if '=' in command_str:
            return self._execute_assignment(command_str)
        return self._execute_function_call(command_str)

    def _execute_assignment(self, command_str):
        parts = command_str.split('=', 1)
        if len(parts) != 2:
            print(f"Invalid assignment: {command_str}")
            return False

        var_name = parts[0].strip()
        value = parts[1].strip()

        if var_name != 'position':
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

        if method_name in self.allowed_methods:
            full_method_name = method_name
        else:
            keyboard_methods = {'press', 'release', 'type'}
            mouse_methods = {'move', 'click', 'scroll', 'position'}

            if method_name in keyboard_methods:
                full_method_name = f'keyboard.{method_name}'
            elif method_name in mouse_methods:
                full_method_name = f'mouse.{method_name}'
            else:
                print(f"Method '{method_name}' not allowed")
                return False

            if full_method_name not in self.allowed_methods:
                print(f"Method '{method_name}' not allowed")
                return False

        args = self._parse_arguments(arg_str)

        try:
            return self.allowed_methods[full_method_name](args)
        except Exception as e:
            print(f"Error executing {command_str}: {e}")
            return False