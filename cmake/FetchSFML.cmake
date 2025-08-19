include(FetchContent)

# Use SFML 2.6.x
FetchContent_Declare(
  sfml
  URL https://github.com/SFML/SFML/archive/refs/tags/2.6.1.zip
)
FetchContent_MakeAvailable(sfml)