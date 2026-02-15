import re
from pynput.keyboard import Key, Controller as KeyboardController
from pynput.mouse import Button, Controller as MouseController

class Controller:
    def __init__(self):
        self.keyboard = KeyboardController()
        self.mouse = MouseController()
        
        # Compile regex patterns once for performance
        self.patterns = {
            'function_call': re.compile(r'^([\w.]+)\((.*)\)$'),
            'tuple': re.compile(r'^\((\d+),\s*(\d+)\)$'),
            'numbers': re.compile(r'^(-?\d+),\s*(-?\d+)$'),
            'single_number': re.compile(r'^(-?\d+)$'),
            'button': re.compile(r'^Button\.(\w+)$'),
            'key': re.compile(r'^Key\.(\w+)$'),
            'string': re.compile(r'^["\'](.*)["\']\s*$'),
        }
        
        # Allowed methods
        self.allowed_methods = {
            # Keyboard methods
            'keyboard.press': self._keyboard_press,
            'keyboard.release': self._keyboard_release,
            'keyboard.type': self._keyboard_type,
            # Mouse methods
            'mouse.move': self._mouse_move,
            'mouse.click': self._mouse_click,
            'mouse.scroll': self._mouse_scroll,
            'mouse.position': self._mouse_position,
            'mouse.press': self._mouse_press,
            'mouse.release': self._mouse_release,
        }
    
    def _keyboard_press(self, args):
        """Handle keyboard press"""
        key = self._parse_key(args[0])
        if key is not None:
            self.keyboard.press(key)
            return True
        return False
    
    def _keyboard_release(self, args):
        """Handle keyboard release"""
        key = self._parse_key(args[0])
        if key is not None:
            self.keyboard.release(key)
            return True
        return False
    
    def _keyboard_type(self, args):
        """Handle typing text"""
        text = self._parse_string(args[0])
        if text is not None:
            self.keyboard.type(text)
            return True
        return False
    
    def _mouse_move(self, args):
        """Handle mouse move - move(dx, dy)"""
        if len(args) != 2:
            return False
        
        try:
            dx = int(args[0])
            dy = int(args[1])
            self.mouse.move(dx, dy)
            return True
        except ValueError:
            return False
    
    def _mouse_position(self, args):
        """Handle mouse position - position = (x, y)"""
        if len(args) != 2:
            return False
        
        try:
            x = int(args[0])
            y = int(args[1])
            self.mouse.position = (x, y)
            return True
        except ValueError:
            return False
    
    def _mouse_click(self, args):
        """Handle mouse click - click(Button.left, count)"""
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
        """Handle mouse scroll - scroll(dx, dy)"""
        if len(args) != 2:
            return False
        
        try:
            dx = int(args[0])
            dy = int(args[1])
            self.mouse.scroll(dx, dy)
            return True
        except ValueError:
            return False
    
    def _mouse_press(self, args):
        """Handle mouse press - press(Button.left)"""
        if len(args) != 1:
            return False
        
        button = self._parse_button(args[0])
        if button is None:
            return False
        
        self.mouse.press(button)
        return True
    
    def _mouse_release(self, args):
        """Handle mouse release - release(Button.left)"""
        if len(args) != 1:
            return False
        
        button = self._parse_button(args[0])
        if button is None:
            return False
        
        self.mouse.release(button)
        return True
    
    def _parse_key(self, key_str):
        """Parse keyboard key"""
        key_str = key_str.strip()
        
        # Handle Key.* constants
        match = self.patterns['key'].match(key_str)
        if match:
            key_name = match.group(1)
            if hasattr(Key, key_name):
                return getattr(Key, key_name)
            else:
                print(f"Unknown key: {key_name}")
                return None
        
        # Handle quoted strings (single character)
        string = self._parse_string(key_str)
        if string is not None and len(string) == 1:
            return string
        
        print(f"Invalid key format: {key_str}")
        return None
    
    def _parse_button(self, button_str):
        """Parse mouse button"""
        button_str = button_str.strip()
        
        # Handle Button.* constants
        match = self.patterns['button'].match(button_str)
        if match:
            button_name = match.group(1)
            if hasattr(Button, button_name):
                return getattr(Button, button_name)
            else:
                print(f"Unknown button: {button_name}")
                return None
        
        print(f"Invalid button format: {button_str}")
        return None
    
    def _parse_string(self, string_str):
        """Parse quoted string"""
        string_str = string_str.strip()
        
        # Match single or double quoted strings
        if (string_str.startswith("'") and string_str.endswith("'")) or \
           (string_str.startswith('"') and string_str.endswith('"')):
            return string_str[1:-1]
        
        return None
    
    def _parse_arguments(self, arg_str):
        """Parse comma-separated arguments"""
        if not arg_str.strip():
            return []
        
        # Split by commas, but respect quotes and parentheses
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
    
    def execute_command(self, command_str):
        """
        Execute a command string
        
        Supported formats:
        - Keyboard: press(Key.space), release('a'), type('Hello')
        - Mouse: move(5, -5), position = (10, 20), click(Button.left, 2), scroll(0, 2)
        """
        command_str = command_str.strip()
        
        # Handle assignment syntax: position = (x, y)
        if '=' in command_str:
            return self._execute_assignment(command_str)
        
        # Handle function call syntax: method(args)
        return self._execute_function_call(command_str)
    
    def _execute_assignment(self, command_str):
        """Handle assignment like: position = (10, 20)"""
        parts = command_str.split('=', 1)
        if len(parts) != 2:
            print(f"Invalid assignment: {command_str}")
            return False
        
        var_name = parts[0].strip()
        value = parts[1].strip()
        
        # Only support 'position' assignment
        if var_name != 'position':
            print(f"Unknown variable: {var_name}")
            return False
        
        # Parse tuple value: (x, y)
        match = self.patterns['tuple'].match(value)
        if not match:
            print(f"Invalid tuple format: {value}")
            return False
        
        x = match.group(1)
        y = match.group(2)
        
        return self._mouse_position([x, y])
    
    def _execute_function_call(self, command_str):
        """Handle function call like: press(Key.space) or mouse.press(Button.left)"""
        match = self.patterns['function_call'].match(command_str)
        
        if not match:
            print(f"Invalid command format: {command_str}")
            return False
        
        method_name = match.group(1)
        arg_str = match.group(2)
        
        # Check if method is allowed (support both 'press' and 'mouse.press' formats)
        if method_name in self.allowed_methods:
            # Direct match (e.g., 'mouse.press')
            full_method_name = method_name
        else:
            # Try prefixing with 'keyboard.' or 'mouse.'
            # Determine which prefix based on method name
            keyboard_methods = {'press', 'release', 'type'}
            mouse_methods = {'move', 'click', 'scroll', 'position'}
            
            if method_name in keyboard_methods:
                full_method_name = f'keyboard.{method_name}'
            elif method_name in mouse_methods:
                full_method_name = f'mouse.{method_name}'
            else:
                # Could be mouse.press, mouse.release which aren't in the simple list
                print(f"Method '{method_name}' not allowed")
                return False
            
            # Verify the prefixed method exists
            if full_method_name not in self.allowed_methods:
                print(f"Method '{method_name}' not allowed")
                return False
        
        # Parse arguments
        args = self._parse_arguments(arg_str)
        
        # Execute the method
        try:
            return self.allowed_methods[full_method_name](args)
        except Exception as e:
            print(f"Error executing {command_str}: {e}")
            return False


# ============================================================================
# USAGE EXAMPLES
# ============================================================================

# if __name__ == "__main__":
#     executor = Controller()
    
#     print("Testing Keyboard Commands:")
#     print("-" * 50)
    
#     keyboard_commands = [
#         "press(Key.space)",
#         "release(Key.space)",
#         "press('a')",
#         "release('a')",
#         "type('Hello World')",
#         "press(Key.shift)",
#         "release(Key.shift)",
#     ]
    
#     for cmd in keyboard_commands:
#         print(f"Executing: {cmd}")
#         success = executor.execute_command(cmd)
#         print(f"Result: {'✓ Success' if success else '✗ Failed'}\n")
    
#     print("\nTesting Mouse Commands:")
#     print("-" * 50)
    
#     mouse_commands = [
#         "position = (100, 200)",
#         "move(5, -5)",
#         "move(-10, 10)",
#         "press(Button.left)",
#         "release(Button.left)",
#         "click(Button.left)",
#         "click(Button.right, 2)",
#         "click(Button.middle, 1)",
#         "scroll(0, 2)",
#         "scroll(-1, 0)",
#     ]
    
#     for cmd in mouse_commands:
#         print(f"Executing: {cmd}")
#         success = executor.execute_command(cmd)
#         print(f"Result: {'✓ Success' if success else '✗ Failed'}\n")
    
#     print("\nTesting Invalid Commands:")
#     print("-" * 50)
    
#     invalid_commands = [
#         "invalid()",
#         "press(UnknownKey)",
#         "click(Button.unknown)",
#         "move(5)",  # Missing argument
#         "position = 100",  # Invalid tuple
#         "eval('malicious code')",
#     ]
    
#     for cmd in invalid_commands:
#         print(f"Executing: {cmd}")
#         success = executor.execute_command(cmd)
#         print(f"Result: {'✓ Success' if success else '✗ Failed'}\n")