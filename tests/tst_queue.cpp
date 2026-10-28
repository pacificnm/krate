#include <QtTest>

#include <cstdint>
#include <thread>

#include "dsp/SpscQueue.h"

using namespace k303;

class TestQueue : public QObject
{
    Q_OBJECT

private slots:
    void fifoOrder()
    {
        SpscQueue<int, 8> q;
        QVERIFY(!q.pop().has_value());
        for (int i = 0; i < 5; ++i)
            QVERIFY(q.push(i));
        for (int i = 0; i < 5; ++i)
            QCOMPARE(*q.pop(), i);
        QVERIFY(!q.pop().has_value());
    }

    void fullQueueRejects()
    {
        SpscQueue<int, 4> q;
        QCOMPARE(q.capacity(), std::size_t(3));
        QVERIFY(q.push(1));
        QVERIFY(q.push(2));
        QVERIFY(q.push(3));
        QVERIFY(!q.push(4)); // dropped, not overwritten
        QCOMPARE(*q.pop(), 1);
        QVERIFY(q.push(4));  // room again after a pop
        QCOMPARE(*q.pop(), 2);
        QCOMPARE(*q.pop(), 3);
        QCOMPARE(*q.pop(), 4);
    }

    void wrapsAround()
    {
        SpscQueue<int, 4> q;
        for (int i = 0; i < 1000; ++i) {
            QVERIFY(q.push(i));
            QCOMPARE(*q.pop(), i);
        }
    }

    // One producer thread, one consumer thread: every item arrives once, in order.
    void concurrentProducerConsumer()
    {
        SpscQueue<std::uint64_t, 64> q;
        constexpr std::uint64_t count = 2'000'000;

        std::thread producer([&q] {
            for (std::uint64_t i = 0; i < count;) {
                if (q.push(i))
                    ++i;
            }
        });

        std::uint64_t expected = 0;
        bool inOrder = true;
        while (expected < count) {
            if (const auto v = q.pop()) {
                inOrder = inOrder && *v == expected;
                ++expected;
            }
        }
        producer.join();
        QVERIFY(inOrder);
        QVERIFY(!q.pop().has_value());
    }
};

QTEST_APPLESS_MAIN(TestQueue)
#include "tst_queue.moc"
