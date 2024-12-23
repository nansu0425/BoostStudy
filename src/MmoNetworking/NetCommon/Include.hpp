#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <optional>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#define WINVER          0x0A00
#define _WIN32_WINNT    0x0A00
#endif // _WIN32

#define ASIO_STANDALONE
#include <boost/asio.hpp>
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
