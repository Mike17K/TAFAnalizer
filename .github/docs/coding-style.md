# Coding Style Guide

## Naming Conventions
- **Classes**: PascalCase (e.g., `NetworkLayer`, `ApplicationInterface`)
- **Variables/Functions**: camelCase (e.g., `networkLayer`, `createTask`)
- **Constants**: UPPER_SNAKE_CASE (e.g., `MAX_BUFFER_SIZE`)
- **Private members**: prefix with underscore `_variableName`

## Comments
- Write comments that explain **why**, not **what**
- Keep comments minimal and descriptive
- Use single-line `//` for brief notes
- Use multi-line `/* */` for section headers

## Code Organization
- One class per file (header + implementation)
- Group related functions together
- Declare variables close to their first use
- Initialize pointers to `nullptr`

## Error Handling
- Log errors to Serial with descriptive messages
- Use `nullptr` checks before dereferencing pointers
- Throw `std::runtime_error` for critical failures only
- Return boolean success/failure for non-critical operations

## Formatting
- Indentation: 2 spaces (no tabs)
- Braces: Opening brace on same line
- Line length: Keep under 120 characters when possible
- Blank lines: One between functions, two between classes
