#include "pjsip/PjsipShutdownAuthorization.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <latch>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

using voip::pjsip::detail::PjsipShutdownAuthorization;
using voip::pjsip::detail::ShutdownAuthorizationState;
using voip::pjsip::detail::ShutdownProof;
using voip::pjsip::detail::ShutdownReceipt;
using Generation = PjsipShutdownAuthorization::Generation;

constexpr std::array<ShutdownProof, 4> proofs{
    ShutdownProof::CallSlotsDrained,
    ShutdownProof::AccountShutdownSucceeded,
    ShutdownProof::LocalCallbacksDrained,
    ShutdownProof::ExternalCallbackIngressQuiescent,
};

void require(const bool condition, const std::string_view message)
{
    if (!condition) {
        throw std::runtime_error(std::string{message});
    }
}

[[nodiscard]] Generation begin_required(PjsipShutdownAuthorization &policy)
{
    const Generation generation = policy.begin_attempt().value_or(0);
    require(generation != 0, "failed to begin authorization attempt");
    return generation;
}

void report_every_proof(PjsipShutdownAuthorization &policy, const Generation generation)
{
    for (const auto proof : proofs) {
        require(policy.report(generation, proof, ShutdownReceipt::Verified),
                "valid proof rejected");
    }
}

void missing_proofs_never_authorize()
{
    PjsipShutdownAuthorization policy;
    require(policy.state() == ShutdownAuthorizationState::Idle, "invalid initial state");
    for (const auto missing : proofs) {
        const Generation generation = begin_required(policy);
        for (const auto proof : proofs) {
            if (proof != missing) {
                require(policy.report(generation, proof, ShutdownReceipt::Verified),
                        "partial proof rejected");
            }
        }
        require(!policy.authorize(generation), "incomplete proof set authorized teardown");
        require(policy.state() == ShutdownAuthorizationState::Collecting,
                "missing proof changed state");
    }
}

void all_failures_are_terminal_for_attempt()
{
    constexpr std::array failures{ShutdownReceipt::Timeout, ShutdownReceipt::Busy,
                                  ShutdownReceipt::Exception, ShutdownReceipt::NotQuiescent};
    for (const auto failure : failures) {
        for (const auto proof : proofs) {
            PjsipShutdownAuthorization policy;
            const Generation generation = begin_required(policy);
            require(policy.report(generation, proof, failure), "failure proof rejected");
            require(policy.state() == ShutdownAuthorizationState::Denied &&
                        policy.failure() == failure,
                    "failure was not retained");
            require(!policy.authorize(generation), "failed attempt authorized teardown");
            require(!policy.report(generation, proof, ShutdownReceipt::Verified),
                    "late success overrode failure");
        }
    }
}

void duplicate_and_invalid_proof_cannot_authorize()
{
    PjsipShutdownAuthorization policy;
    const Generation generation = begin_required(policy);
    require(!policy.report(generation, ShutdownProof::Count, ShutdownReceipt::Verified),
            "count sentinel was accepted as proof");
    const auto invalid_proof = static_cast<ShutdownProof>(255);
    require(!policy.report(generation, invalid_proof, ShutdownReceipt::Verified),
            "invalid proof was accepted");
    require(policy.report(generation, ShutdownProof::CallSlotsDrained, ShutdownReceipt::Verified),
            "first receipt rejected");
    require(!policy.report(generation, ShutdownProof::CallSlotsDrained, ShutdownReceipt::Verified),
            "duplicate proof was accepted");
    require(!policy.authorize(generation), "one receipt authorized teardown");
}

void retry_clears_all_receipts_and_rejects_stale_completions()
{
    PjsipShutdownAuthorization policy;
    const Generation first = begin_required(policy);
    require(policy.report(first, ShutdownProof::CallSlotsDrained, ShutdownReceipt::Verified),
            "first receipt rejected");
    const Generation second = begin_required(policy);
    require(second != first, "retry did not advance generation");
    require(!policy.report(first, ShutdownProof::AccountShutdownSucceeded, ShutdownReceipt::Busy),
            "old failure poisoned current attempt");
    require(!policy.report(first, ShutdownProof::ExternalCallbackIngressQuiescent,
                           ShutdownReceipt::Verified),
            "old callback confirmation crossed retry");
    for (const auto proof : proofs) {
        if (proof != ShutdownProof::CallSlotsDrained) {
            require(policy.report(second, proof, ShutdownReceipt::Verified),
                    "current proof rejected");
        }
    }
    require(!policy.authorize(first) && !policy.authorize(second),
            "mixed generations authorized teardown");
    require(policy.state() == ShutdownAuthorizationState::Collecting && !policy.failure(),
            "stale receipt affected current attempt");
    require(policy.report(second, ShutdownProof::CallSlotsDrained, ShutdownReceipt::Verified),
            "current call-slot receipt rejected");
    require(policy.authorize(second), "complete current attempt not authorized");
}

void every_order_authorizes_exactly_once()
{
    auto order = proofs;
    std::size_t permutations = 0;
    do {
        PjsipShutdownAuthorization policy;
        const Generation generation = begin_required(policy);
        for (const auto proof : order) {
            require(policy.report(generation, proof, ShutdownReceipt::Verified),
                    "valid receipt rejected");
        }
        require(policy.authorize(generation), "four proofs not authorized");
        require(policy.state() == ShutdownAuthorizationState::Authorized,
                "authorization state incorrect");
        require(!policy.authorize(generation), "duplicate authorization succeeded");
        require(!policy.begin_attempt(), "authorized policy reopened");
        const ShutdownProof call_slots = ShutdownProof::CallSlotsDrained;
        require(!policy.report(generation, call_slots, ShutdownReceipt::Verified),
                "authorized policy admitted a late receipt");
        ++permutations;
    } while (std::next_permutation(order.begin(), order.end()));
    require(permutations == 24, "all 24 proof orders were not covered");
}

void late_external_callback_confirmation_cannot_cross_retry()
{
    PjsipShutdownAuthorization policy;
    const Generation first = begin_required(policy);
    std::latch parked{1};
    std::latch resume{1};
    bool accepted = true;
    std::jthread old_callback{[&] {
        parked.count_down();
        resume.wait();
        accepted = policy.report(first, ShutdownProof::ExternalCallbackIngressQuiescent,
                                 ShutdownReceipt::Verified);
    }};
    parked.wait();
    const Generation retry = begin_required(policy);
    resume.count_down();
    old_callback.join();
    require(!accepted, "stale callback receipt was accepted");
    require(!policy.authorize(retry), "late callback authorized new attempt");
    report_every_proof(policy, retry);
    require(policy.authorize(retry), "new attempt could not be authorized");
}

void failed_attempt_can_retry_with_fresh_proofs()
{
    PjsipShutdownAuthorization policy;
    const Generation first = begin_required(policy);
    require(policy.report(first, ShutdownProof::AccountShutdownSucceeded, ShutdownReceipt::Busy),
            "busy failure was rejected");
    require(!policy.authorize(first), "busy attempt authorized teardown");
    const Generation retry = begin_required(policy);
    require(retry != first && !policy.failure(), "retry retained previous failure");
    report_every_proof(policy, retry);
    require(policy.authorize(retry), "successful retry denied");
}

} // namespace

int main()
{
    try {
        missing_proofs_never_authorize();
        all_failures_are_terminal_for_attempt();
        duplicate_and_invalid_proof_cannot_authorize();
        retry_clears_all_receipts_and_rejects_stale_completions();
        every_order_authorizes_exactly_once();
        late_external_callback_confirmation_cannot_cross_retry();
        failed_attempt_can_retry_with_fresh_proofs();
        std::cout << "PASS: production-header shutdown authorization (bookkeeping only; "
                     "live PJSUA2 NOT TESTED)\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
