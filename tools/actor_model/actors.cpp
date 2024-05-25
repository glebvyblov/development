#include "actors.h"
#include "events.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/core/hfunc.h>

static auto ShouldContinue = std::make_shared<TProgramShouldContinue>();

/*
Вам нужно написать реализацию TReadActor, TMaximumPrimeDevisorActor, TWriteActor
*/

/*
Требования к TReadActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup
3. После получения этого сообщения считывается новое int64_t значение из strm
4. После этого порождается новый TMaximumPrimeDevisorActor который занимается вычислениями
5. Далее актор посылает себе сообщение NActors::TEvents::TEvWakeup чтобы не блокировать поток этим актором
6. Актор дожидается завершения всех TMaximumPrimeDevisorActor через TEvents::TEvDone
7. Когда чтение из файла завершено и получены подтверждения от всех TMaximumPrimeDevisorActor,
этот актор отправляет сообщение NActors::TEvents::TEvPoisonPill в TWriteActor

TReadActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)

    NActors::TEvents::TEvWakeup:
        if read(strm) -> value:
            register(TMaximumPrimeDevisorActor(value, self, receipment))
            send(self, NActors::TEvents::TEvWakeup)
        else:
            ...

    TEvents::TEvDone:
        if Finish:
            send(receipment, NActors::TEvents::TEvPoisonPill)
        else:
            ...
*/

class TReadActor : public NActors::TActorBootstrapped<TReadActor> {
public:
    explicit TReadActor(const NActors::TActorId& writer_id)
        : writer_id_(writer_id), not_processed_tasks_(), is_io_stream_empty_()
    {}

    void Bootstrap() {
        Become(&TReadActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

private:
    void ProcessReadData(int64_t value) {
        Register(CreateMaximumPrimeDevisorActor(value, SelfId(), writer_id_).Release());
        ++not_processed_tasks_;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    void ProcessEOF() {
        if (!not_processed_tasks_) {
            Send(writer_id_, std::make_unique<NActors::TEvents::TEvPoisonPill>());
            PassAway();
        }
    }

    void ControlWakeUpEvent() {
        int64_t value;
        if (std::cin >> value) {
            return (void) ProcessReadData(value);
        }
        is_io_stream_empty_ = true;
        ProcessEOF();
    }

    void ControlJobEndEvent() {
        if (--not_processed_tasks_ == 0 && is_io_stream_empty_) {
            ProcessEOF();
        }
    }

    STRICT_STFUNC(StateFunc,
          cFunc(TEvents::TEventJobEnd::EventType, ControlJobEndEvent)
          cFunc(NActors::TEvents::TEvWakeup::EventType, ControlWakeUpEvent)
    );

    const NActors::TActorId writer_id_;
    std::size_t not_processed_tasks_;
    bool is_io_stream_empty_;
};

/*
Требования к TMaximumPrimeDevisorActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActorBootstrapped
2. В конструкторе этот актор принимает:
 - значение для которого нужно вычислить простое число
 - ActorId отправителя (ReadActor)
 - ActorId получателя (WriteActor)
2. В Boostrap этот актор отправляет себе NActors::TEvents::TEvWakeup по вызову которого происходит вызов Handler для вычислений
3. Вычисления нельзя проводить больше 10 миллисекунд
4. По истечении этого времени нужно сохранить текущее состояние вычислений в акторе и отправить себе NActors::TEvents::TEvWakeup
5. Когда результат вычислен он посылается в TWriteActor c использованием сообщения TEvWriteValueRequest
6. Далее отправляет ReadActor сообщение TEvents::TEvDone
7. Завершает свою работу

TMaximumPrimeDevisorActor
    Bootstrap:
        send(self, NActors::TEvents::TEvWakeup)

    NActors::TEvents::TEvWakeup:
        calculate
        if > 10 ms:
            Send(SelfId(), NActors::TEvents::TEvWakeup)
        else:
            Send(WriteActor, TEvents::TEvWriteValueRequest)
            Send(ReadActor, TEvents::TEvDone)
            PassAway()
*/


class TMaximumPrimeDevisorActor : public NActors::TActorBootstrapped<TMaximumPrimeDevisorActor> {
public:
    TMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId &reader_id, const NActors::TActorId &writer_id)
        : current_i_(2),
          current_value_(value),
          answer_(),
          initial_value_(value),
          reader_id_(reader_id),
          writer_id(writer_id)
    {}

    void Bootstrap() {
        Become(&TMaximumPrimeDevisorActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

private:
    bool CheckTimeout(const auto &expire_time) const {
        if (std::chrono::system_clock::now() >= expire_time) {
            Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
            return true;
        }
        return false;
    }

    void ControlWakeUpEvent() {
        const auto expire_time = std::chrono::system_clock::now() + timeout;
        for (; current_value_ > 1 && current_i_ * current_i_ <= initial_value_; ++current_i_) {
            while (current_value_ % current_i_ == 0) {
                answer_ = current_i_;
                current_value_ /= current_i_;
                if (CheckTimeout(expire_time)) {
                    return;
                }
            }
            if (CheckTimeout(expire_time)) {
                return;
            }
        }
        answer_ = std::max(answer_, current_value_);
        Send(writer_id, std::make_unique<TEvents::TEventWrite>(answer_));
        Send(reader_id_, std::make_unique<TEvents::TEventJobEnd>());
        PassAway();
    }

    STRICT_STFUNC(StateFunc,
          cFunc(NActors::TEvents::TEvWakeup::EventType, ControlWakeUpEvent)
    );

    constexpr static auto timeout = std::chrono::milliseconds(10);

    int64_t current_i_;
    int64_t current_value_;
    int64_t answer_;
    const int64_t initial_value_;
    const NActors::TActorId reader_id_;
    const NActors::TActorId writer_id;
};

/*
Требования к TWriteActor:
1. Рекомендуется отнаследовать этот актор от NActors::TActor
2. Этот актор получает два типа сообщений NActors::TEvents::TEvPoisonPill::EventType и TEvents::TEvWriteValueRequest
2. В случае TEvents::TEvWriteValueRequest он принимает результат посчитанный в TMaximumPrimeDevisorActor и прибавляет его к локальной сумме
4. В случае NActors::TEvents::TEvPoisonPill::EventType актор выводит в Cout посчитанную локальнкую сумму, проставляет ShouldStop и завершает свое выполнение через PassAway

TWriteActor
    TEvents::TEvWriteValueRequest ev:
        Sum += ev->Value

    NActors::TEvents::TEvPoisonPill::EventType:
        Cout << Sum << Endl;
        ShouldStop()
        PassAway()
*/

class TWriteActor : public NActors::TActor<TWriteActor> {
public:
    TWriteActor()
        : NActors::TActor<TWriteActor>(&TWriteActor::StateFunc), summary_()
    {}

private:
    void ControlWriteMessageEvent(const TEvents::TEventWrite::TPtr& ptr) {
        summary_ += ptr->Get()->GetValue();
    }

    void ControlPoisonPill() {
        std::cout << summary_ << std::endl;
        ShouldContinue->ShouldStop();
        PassAway();
    }

    STRICT_STFUNC(StateFunc,
          hFunc(TEvents::TEventWrite, ControlWriteMessageEvent)
          cFunc(NActors::TEvents::TEvPoisonPill::EventType, ControlPoisonPill)
    );

    int64_t summary_;
};

class TSelfPingActor : public NActors::TActorBootstrapped<TSelfPingActor> {
    TDuration Latency;
    TInstant LastTime;

public:
    TSelfPingActor(const TDuration& latency)
        : Latency(latency)
    {}

    void Bootstrap() {
        LastTime = TInstant::Now();
        Become(&TSelfPingActor::StateFunc);
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }

    STRICT_STFUNC(StateFunc, {
        cFunc(NActors::TEvents::TEvWakeup::EventType, HandleWakeup);
    });

    void HandleWakeup() {
        auto now = TInstant::Now();
        TDuration delta = now - LastTime;
        Y_VERIFY(delta <= Latency, "Latency too big");
        LastTime = now;
        Send(SelfId(), std::make_unique<NActors::TEvents::TEvWakeup>());
    }
};

THolder<NActors::IActor> CreateReadActor(const NActors::TActorId& writer_id) {
    return MakeHolder<TReadActor>(writer_id);
}

THolder<NActors::IActor> CreateMaximumPrimeDevisorActor(int64_t value, const NActors::TActorId& reader_id, const NActors::TActorId& writer_id) {
    return MakeHolder<TMaximumPrimeDevisorActor>(value, reader_id, writer_id);
}

THolder<NActors::IActor> CreateWriteActor() {
    return MakeHolder<TWriteActor>();
}

THolder<NActors::IActor> CreateSelfPingActor(const TDuration& latency) {
    return MakeHolder<TSelfPingActor>(latency);
}

std::shared_ptr<TProgramShouldContinue> GetProgramShouldContinue() {
    return ShouldContinue;
}
