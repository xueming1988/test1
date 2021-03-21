#ifndef METRIC_OBSERVER_INTERFACE_H_
#define METRIC_OBSERVER_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enum used to define Unit MetricDatum in CloudWatch.
 * https://docs.aws.amazon.com/AmazonCloudWatch/latest/APIReference/API_MetricDatum.html
 */
typedef enum MetricUnit {
    Bits,
    BitsSecond,
    Bytes,
    BytesSecond,
    Count,
    CountSecond,
    Gigabits,
    GigabitsSecond,
    Gigabytes,
    GigabytesSecond,
    Kilobits,
    KilobitsSecond,
    Kilobytes,
    KilobytesSecond,
    Megabits,
    MegabitsSecond,
    Megabytes,
    MegabytesSecond,
    Microseconds,
    Milliseconds,
    None,
    Percent,
    Seconds,
    Terabits,
    TerabitsSecond,
    Terabytes,
    TerabtyesSecond
} MetricUnit;

/**
 * Enum used to define metric type.
 */
typedef enum MetricType {
    CounterMetric,
    TimerMetric
} MetricType;

/**
 * Enum used to define metric priority.
 */
typedef enum MetricPriority {
    NORMAL,
    HIGH,
    CRITICAL
} MetricPriority;

typedef struct {
    MetricType type;
    const char* name;
    double value;
    MetricUnit unit;
} MetricEventEntry;

typedef struct {
    const char* name;
    const char* value;
} MetricMetadataEntry;

typedef struct MetricObserverInterface {
    /**
    * Observer interface for listening to record metrics.
    */
    void (*onRecordMetric)(const char* program, const char* source, MetricEventEntry metricEvent[],
        MetricMetadataEntry metricMetadata[], MetricPriority metricPriority, double timestamp);

#ifdef __cplusplus
    bool operator==(const struct MetricObserverInterface& other) const {
        return onRecordMetric == other.onRecordMetric;
    }

    bool isValid() const {
        return onRecordMetric != nullptr;
    }

#endif
} MetricObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // METRIC_OBSERVER_INTERFACE_H_
