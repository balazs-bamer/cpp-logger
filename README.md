# C++ template-based cross-platform log library

```C++
// Stripped template instantiations.
nowtech::log::LogConfig logConfig;
LogSender::init(&std::cout);
Log::init(logConfig);
Log::registerTopic(nowtech::LogTopics::system, "system");
Log::registerCurrentTask("main");

//...

Log::i(nowtech::LogTopics::system) << "bulk data size:" << someCollection.size() << Log::end;  // one group of 2 items

auto logger = Log::n<Log::debug>() << "bulk data follows:"; // one group of many items starts
for(auto item : someCollection) {
  logger << LC::X4 << item;                     // format: hexadecimal, fill to 4 digits
}
logger << Log::end;                             // the group of many items ends, or when logger goes out of scope

//...

Log::unregisterCurrentTask();
Log::done();
```

This is a complete rework of my [old logger](https://github.com/balazs-bamer/cpp-logger/tree/old/) after identifying the design flaws and performance problems. The library requires C++17.

The code was written to possibly conform to MISRA C++ and High-Integrity C++.

The library is published under the [MIT license](https://opensource.org/licenses/MIT).

Copyright 2020 Balázs Bámer.

## Aims

- Provide a common logger API for cross-platform embedded / desktop code.
- Use as minimal code and data space as possible. I'm not sure if I was good enough to obtain it. On STM32 Cortex M3, flash overhead of direct sending mode is **4k**, queued multithreading mode id **6k**. Anyway, no more time to further develop this library.
- The library was designed to allow the programmer control of log bandwidth usage, program and data memory consumption.
- Log arithmetic C++ types and strings with or without copying them (beneficial for literals).
  - String literals or string constants can be transferred using their pointers to save MCU cycles.
  - More transient strings will be copied instantly.
- Operate in a _strictly typed_ way as opposed to the printf-based loggers some companies still use today.
- We needed a much finer granularity than the usual log levels, so I've introduced independently selectable topics. Recently, I've also added traditional log levels.
- Provide blazing-fast atomic logging of one integral type into a buffer. Its contents can later be converted and sent.
- An important aim was to let the user log a _group of related items_ without other tasks interleaving the output of converting these items.
- Possibly minimal reliance on external libraries. It does not depend on numeric-to-string conversion functions.
- In contrast with the original solution, the new one extensively uses templated classes to
  - implement a flexible plugin architecture for adapting to different user needs
  - let the compiler optimizer do its finest.
  - _totally eliminate_ logging code generation with at least -O1 optimization level with a one-line change (template logic, without #ifdef).
- Let the user decide to rely on C++ exceptions or implement a different kind of error management.
- Let me learn about recent language features like C++20 concepts. Well, I found out that these are not designed for specifying Java-like interfaces for templated classes. At least I couldn't formulate them.

## Architecture

The logger consists of six classes:
- `Log` - the main class providing API and base architecture. This accepts the other ones as template parameters.
- _Queue_ - used to transfer the converting and sending operations from the current task to a background one.
- _Converter_ - converts the user types to strings or any other binary format to be sent or stored.
- _Sender_ - the class responsible for transmitting or storing the converted data. Its `send()` must be `noexcept` and report failures to the _App interface_'s `error()` instead. The STL _App interface_ counts them, see its `getErrorCount()`.
- _App interface_ - used to interface the application, STL and OS. This provides
  - Possibly custom memory management needed by embedded applications.
  - Task management, including
    - Obtaining byte task IDs required by the Log class.
    - Registering and unregistering the current task.
    - Obtaining the task name in C string.
  - Obtaining timestamps using some OS / STL time routine.
  - Error management.
- _Message_ - used as a transfer medium in the Queue. For numeric types and strings transferred by pointers, one message is needed per item. For stored strings, several messages may be necessary. There are two general-purpose implementations:
  - a variant-based message.
  - a more space-efficient handcrafted message.
- _AtomicBuffer_ - provides a buffer and instrumentation for the atomic logging. This bypasses all the queues and logic used to log groups of various types.
- _Config_ - provides some configuration parameters.

The logger can operate in two modes:
- Direct without queue, when the conversion and sending happen without message instantiation and queue usage. This can be useful for single-threaded applications. Lines from different tasks don't mix: a line holds the _App interface_ lock from its first item until it ends, so a long-lived named helper (see below) blocks direct logging in all other tasks.
- With a queue, when each item is sent, it will form one or more (in case of stored strings) messages and use a central thread-safe queue. On the other end of the queue, a background thread pops the messages and groups them by tasks in secondary lists. When a group of related items from a task has arrived, its conversion and sending begin.

## Implementations

The current library offers simple implementations for STL and Boost-based desktop and server applications, as well as FreeRTOS-based embedded applications. The whole implementation assumes an architecture of at least 32 bits.

### Log

This is a general implementation, but tailored for embedded requirements.

In queue-less mode, conversion and sending happen immediately for each item. Thus ,it is desirable that the Sender has some sort of buffering inside.

For the queue mode, it contains a secondary list or queue for each task, which gathers the items in each logged group. After the terminal item arrives, conversion occurs for each item in the sender buffer, and then the sending process begins. These secondary queues are backed by a pool allocator to avoid repeated dynamic memory access.

### ConverterCustomText

A simple converter emitting character strings, with an emphasis on space-efficient operation on embedded platforms. Features:
- Arbitrary numeric base from 2 to 16 for integer conversion.
- Floating-point conversion always happens in scientific mode with NAN and INF distinction (bug: no signs displayed for them).
- Adjustable
  - zero fill
  - numeric base prefix display for binary and hexadecimal
  - extra space before positive numbers to be aligned with negatives
- Automatically adds space between items of a group.

### AppInterfaceFreeRtosMinimal

This implementation assumes FreeRTOS 10.0.1, but should also work with versions 9.x or possibly even older. The main objective was to keep it as simple and small as possible. It provides a global overload of new and delete operators using FreeRTOS' dynamic memory management, but itself uses only a statically allocated array. It uses a linear array for the task registry and omits unregistering, as a typical embedded application typically creates all tasks beforehand and never kills them. Task names are native to FreeRTOS. For similar reasons, logger shutdown is not implemented.
There is a design flaw in the library: checking for ISRs happens here, so the FreeRTOS implementation contains an MCU-specific function, here for STM32. However, it is easy to replace it with the one for the actual MCU.

### AppInterfaceStd

This is a general, desktop-oriented C++17 STL implementation that prioritises speed over space. It uses a hash set and thread-local storage for task registration, and task unregistration is also supported. The task registry API is protected by a mutex. Other, more frequently called functions work without locking. Logger initialisation and shutdown are properly implemented.

### QueueVoid

An empty implementation is used as a placeholder when no queue is required.

### QueueFreeRtos

It utilises FreeRTOS's built-in queue, so no locking is required for sending. Sending an item into the queue can occur from an ISR.

### QueueStdBoost

This one uses a multi-producer, multi-consumer lock-free queue of Boost, so no locking is needed either.

### QueueStdCircular

This one uses a simple circular buffer with `std::lock_guard`.

### SenderVoid

Empty implementation for the case when all the log calls have to be eliminated from the binary. This occurs at GCC and Clang optimisation levels -Os, -O1, -O2, and -O3. The application can utilise a template metaprogramming technique to declare a Log, using this as the appropriate parameter, so no #ifdef is required.

### SenderStmHalMinimal

This is a minimal implementation for the STM32 UART using blocking HAL transmission. A more sophisticated approach would utilise double buffering with DMA to optimise the MCU's efficiency and allow for buffering new data during the potentially slow transmission of the old buffer. This mechanism is implemented in the old version.

### SenderStdOstream

It is a simple std::ostream wrapper.

### SenderRos2

A simple ROS2 log wrapper. This wrapper has its own log levels and every property defined here. However, due to the architecture of this library, this wrapper uses only a compile-time hardwired ROS2 log level.

### AtomicBufferOperational

Normal cross-platform implementation.

### AtomicBufferVoid

Used the normal one instead to strip its static variables when not in use.

### Config

Converts the template arguments into public static variables. One can use it or write a template-less direct class instead, using this example:

```C++
template<bool tAllowRegistrationLog, LogTopic tMaxTopicCount, TaskRepresentation tTaskRepresentation, size_t tDirectBufferSize, int32_t tRefreshPeriod, ErrorLevel tErrorLevel = ErrorLevel::All>
struct Config final {
public:
  static constexpr bool               csAllowRegistrationLog = tAllowRegistrationLog;
  static constexpr LogTopic           csMaxTopicCount        = tMaxTopicCount;
  static constexpr TaskRepresentation csTaskRepresentation   = tTaskRepresentation;
  static constexpr size_t             csDirectBufferSize     = tDirectBufferSize;
  static constexpr int32_t            csRefreshPeriod        = tRefreshPeriod; // Can represent 1s even if the unit is ns.
  static constexpr ErrorLevel         csErrorLevel           = tErrorLevel;
};
```

## Benchmarks

I used [picobench](https://github.com/iboB/picobench) for benchmarks using the supplied benchmark apps. Here are some averaged results measured over 8192 iterations on my Intel(R) Xeon(R) CPU E31220L @ 2.20GHz. Compiled using `clang++ -O2`. There were no significant differences for `MessageVariant` or `MessageCompact`, or whether the task ID was provided or not. Log activity was
- print header
  - print task name
  - print timestamp
- print the logged string

Each value is the average time required for _one_ log call in nanoseconds. Each result is an average of 8 runs, each having 262144 iterations.

|Scenario                 |Constant string (no copy)|Transient string (copy)|
|-------------------------|------------------------:|----------------------:|
|direct                   |230                      |230                    |
|Lockfree queue           |310                      |500                    |
|Circular buffer and lock |430                      |590                    |

Here are benchmark results of atomic logging `int32_t` values, measured on 2097152 iterations. Note that this does not include offline buffer sending, so it is the same for any logger mode:

|Scenario                 |Atomic (ns)|
|-------------------------|----------:|
|any                      |7          |


## Space requirements

I have investigated several scenarios using simple applications which contain practically nothing but a task creation apart of the logging. This way, I could measure the net space required by the log library and its necessary supplementary functions, such as std::unordered_set or floating-point emulation for log10.

For desktop, I used clang version 10.0.0 on x64. For embedded development, I used arm-none-eabi-g++ 10.1.1 on an STM32 Cortex-M3. This MCU requires emulation for floating-point operations. All measurements were conducted using -Os. I present the net growth in segments, text, data and BSS for each of the following scenarios:
- direct logging (for single-threaded applications)
- logging turned off with SenderVoid
- logging with queue using MessageVariant (for multithreaded applications)
- logging with queue using MessageCompact (for multithreaded applications)

### FreeRTOS with floating point

To obtain net results, I put some floating-point operations in the application *test-sizes-freertosminimal-float.cpp* because a real application would use them apart of logging. 

|Atomic|Mode              |   Text|  Data|    BSS|
|------|------------------|------:|-----:|------:|
|-     |off               |0      |0     |0      |
|off   |direct            |13072  |108   |56     |
|on    |direct            |13400  |108   |80     |
|off   |multitask, variant|15272  |108   |88     |
|on    |multitask, variant|16000  |108   |112    |
|off   |multitask, compact|14984  |108   |88     |
|on    |multitask, compact|15712  |108   |112    |

### FreeRTOS without floating point

No floating-point arithmetic is used in the application, and support is turned off in the logger. Source is *test-sizes-freertosminimal-nofloat.cpp*. Note, only this table contains values from the _only atomic logging_ scenario, since atomic logging assumes only integral types.

|Atomic|Mode              |   Text|  Data|    BSS|
|------|------------------|------:|-----:|------:|
|-     |off               |0      |0     |0      |
|only  |direct *          |3704   |4     |72     |
|off   |direct            |4212   |8     |60     |
|on    |direct            |4540   |8     |76     |
|off   |multitask, variant|6412   |8     |84     |
|on    |multitask, variant|7140   |8     |108    |
|off   |multitask, compact|6140   |8     |84     |
|on    |multitask, compact|6868   |8     |108    |

* = When only using atomic logging, multitasking mode is meaningless.

## API

### Supported types

As all major coding standards suggest, the use of integer types with indeterminate size is discouraged; therefore, this library does not support `long` or `unsigned`.

`LogFormat` is a struct holding the numeric base and the fill value. It is easiest to reach in `LogConfig`.

|C++ type       |LogFormat prefix affects it|Remark                           |
|---------------|---------------------------|---------------------------------|
|`bool`         |no                         |Appears as _true_ or _false_ in the current `LogConverterCustomText` implementation.|
|`float`        |mantissa precision         |Enabled only if floating point support is on.|
|`double`       |mantissa precision         |Enabled only if floating point support is on and the payload is chosen to be big enough.|
|`long double`  |mantissa precision         |Enabled only if floating point support is on and the payload is chosen to be big enough.|
|`uint8_t`      |base and fill              ||
|`uint16_t`     |base and fill              ||
|`uint32_t`     |base and fill              ||
|`uint64_t`     |base and fill              ||
|`int8_t`       |base and fill              ||
|`int16_t`      |base and fill              ||
|`int32_t`      |base and fill              ||
|`int64_t`      |base and fill              ||
|`char`         |no                         |This and the strings support only plain old 8-bit ASCII.|
|`char const *` |for no prefix              |Can be of arbitrary length for string constants.|
|`char const *` |for `LC::St` prefix        |Only a limited length of _payload size_ * 255 characters can be transferred from a transient string.|
|`std::string`  |no                         |Implicitely adds `LC::St` prefix, since `std::string` instances are usually mutable.|

64-bit integer types require emulation on 32-bit architectures. By default, `LogConverterCustomText`'s templated conversion routine uses 32-bit numbers to gain speed. Using 64-bit operands instantiates the 64-bit emulated routines as well, which takes extra flash space on embedded.

### Initialization

Log system initialisation consists of the following steps:
1. Declare possible topics (see next section).
2. Instantiate templates.
3. Define and fill `LogConfig` struct. All its fields have a default value.
4. Initialise the _Sender_. This has implementation-specific arguments for the actual output.
5. Initialize the _Log_ using the _LogConfig_ instance.
6. Register the required topics (see next section).
7. Call `Log::registerCurrentTask("task name");` for  each interested task. Other tasks won't be able to log.

Refer to the beginning for an example of STL without the first step. Here is a FreeRTOS template declaration without floating point support, but for multithreaded mode:

```C++
constexpr nowtech::log::TaskId cgMaxTaskCount = cgThreadCount + 1;
constexpr bool cgAllowRegistrationLog = true;
constexpr bool cgLogFromIsr = false;
constexpr size_t cgTaskShutdownSleepPeriod = 100u;
constexpr bool cgArchitecture64 = true;
constexpr uint8_t cgAppendStackBufferSize = 100u;
constexpr bool cgAppendBasePrefix = true;
constexpr bool cgAlignSigned = false;
using AtomicBufferType = int32_t;
constexpr size_t cgAtomicBufferExponent = 14u;
constexpr AtomicBufferType cgAtomicBufferInvalidValue = 1234546789;
constexpr size_t cgTransmitBufferSize = 123u;
constexpr size_t cgPayloadSize = 14u;
constexpr bool cgSupportFloatingPoint = true;
constexpr size_t cgQueueSize = 444u;
constexpr nowtech::log::LogTopic cgMaxTopicCount = 2;
constexpr nowtech::log::TaskRepresentation cgTaskRepresentation = nowtech::log::TaskRepresentation::cName;
constexpr size_t cgDirectBufferSize = 0u;
constexpr nowtech::log::ErrorLevel cgErrorLevel = nowtech::log::ErrorLevel::Error;

using LogAppInterface = nowtech::log::AppInterfaceStd<cgMaxTaskCount, cgLogFromIsr, cgTaskShutdownSleepPeriod>;
constexpr typename LogAppInterface::LogTime cgTimeout = 123u;
constexpr typename LogAppInterface::LogTime cgRefreshPeriod = 444;
using LogMessage = nowtech::log::MessageCompact<cgPayloadSize, cgSupportFloatingPoint>;
using LogConverter = nowtech::log::ConverterCustomText<LogMessage, cgArchitecture64, cgAppendStackBufferSize, cgAppendBasePrefix, cgAlignSigned>;
using LogSender = nowtech::log::SenderStdOstream<LogAppInterface, LogConverter, cgTransmitBufferSize, cgTimeout>;
using LogQueue = nowtech::log::QueueStdBoost<LogMessage, LogAppInterface, cgQueueSize>;
using LogAtomicBuffer = nowtech::log::AtomicBufferOperational<LogAppInterface, AtomicBufferType, cgAtomicBufferExponent, cgAtomicBufferInvalidValue>;
using LogConfig = nowtech::log::Config<cgAllowRegistrationLog, cgMaxTopicCount, cgTaskRepresentation, cgDirectBufferSize, cgRefreshPeriod, cgErrorLevel>;
using Log = nowtech::log::Log<LogQueue, LogSender, LogAtomicBuffer, LogConfig>;
```

Explanation of configuration parameters:
|Name in the library source                                |Goes in                  |Remark             |
|----------------------------------------------------------|-------------------------|-------------------|
|`TaskId tMaxTaskCount`                                    |_App interface_          |TaskId is `uint8_t`. Maximum value is 254.|
|`bool tLogFromIsr`                                        |_App interface_          |Determines if logging from ISR is enabled (when applicable).|
|`size_t tTaskShutdownPollPeriod`                          |_App interface_          |Polling interval in implementation-defined unit (usually ms) for log system shutdown.|
|`size_t tPayloadSize`                                     |_Message_                |Maximum size of payload in bytes.|
|`bool tSupportFloatingPoint`                              |_Message_                |Floating point support.|
|`typename tMessage`                                       |_Converter_              |The _Message_ type to use.|
|`bool tArchitecture64`                                    |_Converter_              |Tells if we are on 64-bit architecture (if not, the 32-bit). Well, it could have been figured out.|
|`uint8_t tAppendStackBufferSize`                          |_Converter_              |Size of stack buffer used for binary to text conversion.|
|`bool tAppendBasePrefix`                                  |_Converter_              |If true base-2 or base-16 conversion should prepend _0b_ or _0x_.|
|`bool tAlignSigned`                                       |_Converter_              |If true, positive numbers will get an extra ' ' to be aligned with negatives. |
|`typename tAppInterface`                                  |_Sender_                 |The _app interface_ type to use.|
|`typename tConverter`                                     |_Sender_                 |The _Converter_ type to use.|
|`size_t tTransmitBufferSize`                              |_Sender_                 |Length of buffer to use for conversion. This should be sufficient for the joint size of the largest group of possible items.|
|`typename tAppInterface::LogTime tTimeout`                |_Sender_                 |Timeout in implementation-defined unit (usually ms) for transmission.|
|`typename tMessage`                                       |_Queue_                  |The _Message_ type to use.|
|`typename tAppInterface`                                  |_Queue_                  |The _app interface_ type to use.|
|`size_t tQueueSize`                                       |_Queue_                  |Number of items the queue should hold. This applies to the master queue and to the aggregated capacity of the per-task queues.|
|`typename tAppInterface`                                  |`AtomicBufferOperational`|The _app interface_ to use.|
|`typename tAtomicBufferType`                              |`AtomicBufferOperational`|The type to log as atomic; only integral types are allowed.|
|`tAtomicBufferSizeExponent`                               |`AtomicBufferOperational`|Exponent of the buffer size (base of the power is 2). For numeric reasons, the buffer size is always a power of 2.|
|`tAtomicBufferType tInvalidValue`                         |`AtomicBufferOperational`|Invalid value, which won't be sent when all the buffer contents are being sent.|
|`typename tQueue`                                         |`Log`                    |The _Queue_ type to use.|
|`typename tSender`                                        |`Log`                    |The _Sender_ type to use.|
|`typename tAtomicBuffer`                                  |`Log`                    |The _AtomicBuffer_ type to use.|
|`typename tLogConfig`                                     |`Log`                    |The _LogConfig_ type to use.|
|`bool allowRegistrationLog`                               |`Config`                 |True if task (un)registering should be logged.|
|`LogTopic tMaxTopicCount`                                 |`Config`                 |LogTopic is `int8_t`. Maximum is 127.|
|`TaskRepresentation tTaskRepresentation`                  |`Config`                 |One of `cNone` (for omitting it), `cId` (for numeric task ID), `cName` (for task name).|
|`size_t tDirectBufferSize`                                |`Config`                 |When 0, the given _Queue_ will be used. Otherwise, it is the size of a buffer on the stack to hold a converted item before sending it.|
|`int32_t tRefreshPeriod`                                  |`Config`                 |Timeout in implementation-defined unit (usually ms) for waiting on the queue before sending what is already present.|
|`ErrorLevel tErrorLevel`                                  |`Config`                 |The application log level with the default value `ErrorLevel::All`.|
|`LogFormat atomicFormat`                                  |`LogFormatConfig`        |Format used for converting the bulk data in the _AtomicBuffer_.
|`LogFormat taskIdFormat`                                  |`LogFormatConfig`        |Format of task ID to use when `tTaskRepresentation == TaskRepresentation::cId`.|
|`LogFormat tickFormat`                                    |`LogFormatConfig`        |Format for displaying the timestamp in the header, if any. Should be `LogConfig::cInvalid` to disable tick output.|
|`LogFormat defaultFormat`                                 |`LogFormatConfig`        |Default formatting, initially `LogConfig::Fm` to obtain maximum possible precision for floating point types.|

### Topics and log levels

The log system supports individually selectable topics to log. I've decided so because we needed a much finer granularity than the traditional log levels. These topics must be declared first, after which they will still be disabled.

```C++
namespace nowtech::LogTopics {
nowtech::log::TopicInstance level1;
nowtech::log::TopicInstance level2;
nowtech::log::TopicInstance level3;
nowtech::log::TopicInstance someTopic;
nowtech::log::TopicInstance someOtherTopic;
}
```

To enable some of them, the interesting ones must be registered in the log system. When the log system receives the `LogTopic` parameter, it will be logged regardless of the actual application log level.

There are three possibilities to use log levels.

#### Native log levels

The `Config` class takes an optional last template argument, the application log level, with the default value `ErrorLevel::All`. The `Log::n(...)` and `Log::i(...)` functions' overloads without operands and taking a `TaskId` (see below) are templated methods with a default value of `ErrorLevel::Off`. A call logs when its level is not above the application log level. `ErrorLevel::Off` is the lowest, so a call without a level always logs, and the application log level `ErrorLevel::Off` keeps only those calls. By providing template values for these methods, the given instantiation chooses between a functional and an empty return value. As the empty one will be fully optimised out for unused log levels, this is more performant than the next one. The log system has these predefined log levels in `Log`:

```C++
static constexpr ErrorLevel fatal = ErrorLevel::Fatal;
static constexpr ErrorLevel error = ErrorLevel::Error;
static constexpr ErrorLevel warn  = ErrorLevel::Warning;
static constexpr ErrorLevel info  = ErrorLevel::Info;
static constexpr ErrorLevel debug = ErrorLevel::Debug;
static constexpr ErrorLevel all   = ErrorLevel::All;
```

So logging happens like

```C++
Log::i<Log::debug>() << "x:" << LC::D4 << posX << "y:" << LC::D4 << posY << Log::end;
```

#### Emulated log levels

For topic declarations, please refer to the section above. This method suits better architectures with limited flash space, since it requires no internal method instantiation for each log level used. However, checking topics is a runtime process, and total elimination of unused log statements won't happen for optimising compilation.

```C++
#ifdef LEVEL1
Log::registerTopic(nowtech::LogTopics::level1, "level1");
#elif LEVEL2
Log::registerTopic(nowtech::LogTopics::level1, "level1");
Log::registerTopic(nowtech::LogTopics::level2, "level2");
#elif LEVEL3
Log::registerTopic(nowtech::LogTopics::level1, "level1");
Log::registerTopic(nowtech::LogTopics::level2, "level2");
Log::registerTopic(nowtech::LogTopics::level3, "level3");
#endif
```

#### Variadic macros

C++20 features suitable variadic macros in the preprocessor, enabling the use of the folding expression API to define preprocessor-implemented log levels. I didn't implement it. This would mean a perfect solution from a performance and space point of view.

### Logging

#### Group logging

All logging APIs are implemented as static functions in the `Log` template class. Logging happens using a `std::ostream` -like API, like in the example at the beginning. There are two overloaded functions to start the chain:
- `static LogShiftChainHelper i(...) noexcept` writes any header configured for the application.
- `static LogShiftChainHelper n(...) noexcept` omits this header, just writes the actual stuff it receives using `<<`.
Note, LogShiftChainHelper implementation depends on the given log mode (direct / queued / shut off).

Each function has four overloads with the following parameter signatures:
- `()` - logs unconditionally, and queries the task ID.
- `(TaskId const aTaskId)` - logs unconditionally using the supplied task ID.
- `(LogTopic const aTopic)` - logs depending on the given task ID were registered, and queries the task ID.
- `(LogTopic const aTopic, TaskId const aTaskId)` - logs depending on the given task ID were registered using the supplied task ID.

One can use the `static TaskId getCurrentTaskId() noexcept` function to query the current task ID and store it. This can be important if querying the task ID is expensive on the given platform.

If you have many or an unknown number of items to log, you can use the form

```C++
auto logger = Log::n() << "someCollection contents:";
for(auto item : someCollection) {
  logger << item;
}
logger << Log::end;  // the group of many items ends
```

A group also ends when its helper is destroyed, so `Log::end` is optional:
- A temporary helper ends at the end of its statement: `Log::n() << "x:" << x;` is a complete group. With `Log::end` it ends at the same point.
- A named helper ends at `Log::end`, or at the end of its scope.
- Helpers are move-only, moving one hands the group over. `auto logger = Log::n() << "x";` moves the temporary into `logger`.
- Always use `auto` for a named helper. `auto&` on a chain result doesn't compile, and `auto&& logger = Log::n() << "x";` compiles but refers to the temporary, which ends its group and dies at the end of the statement.
- Several temporary helpers in one statement end in reverse order at its end, for example `Log::n() << a << Log::end, Log::n() << b;` logs the line of `b` first.
- Don't start another group in the same task while a named helper is open. In queued mode the two groups corrupt each other, and in direct mode the open helper holds the output lock. Unless `NDEBUG` is defined, a debug check reports this to the _App interface_'s `error()` as `Exception::cOverlappingGroups`. The STL _App interface_ counts it, the FreeRTOS one stops in its debug trap. Define `NOWTECH_LOG_CHECK_GROUPS` as 0 or 1 to choose, with the same value in every translation unit. Groups logged from interrupts aren't checked, since interrupts share a task ID.

I've implemented a function call-like entry point using C++17 folding expressions. To be honest, this is just a _why not_ solution, and not an integral part of the API. It gets called like

```C++
Log::f(Log::i(nowtech::LogTopics::someTopic), some, variables, follow);
```

and apart from being clumsy, it even takes more binary space than the `std::ostream` -like API it uses under the hood. It appends `Log::end` automatically.

#### Atomic logging

Atomic logging can happen any time in this way (now the buffer is of type `int16_t`):

```C++
int16_t value = 13;
Log::pushAtomic(value);
```

Of course, this is only effective if the logging is on and the `AtomicBufferOperational` class is being used. These values are accepted from any task or ISR and land in a circular buffer, which is continuously overwritten. To extract the contents, the application should first stop calling `Log::pushAtomic`, as the slower readout would produce undefined behaviour when it is still being written to. (There is no locking for maximum performance.) Then, a task with a valid log registry should call

```C++
Log::sendAtomicBuffer();
```

which is a blocking call. Concurrent calls are serialized, each caller waits for its own complete send. Note that in multithreaded mode, sending from all other tasks will not occur; only the queues will hold messages from concurrent logging as long as possible.
