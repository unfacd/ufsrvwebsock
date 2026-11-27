#
# This is a placeholder reference. Actual script can be fetched via this command:
#
# git archive --remote=ssh://git@tig.unfacd.io/ufsrvcorelib.git HEAD dev/scripts/cpu_affinity_verify.sh | tar -xO > cpu_affinity_verify.sh && chmod +x cpu_affinity_verify.sh
# Or
# d=$(mktemp -d) && trap 'rm -rf "$d"' EXIT && git clone -q --depth 1 --filter=blob:none --sparse ssh://git@tig.unfacd.io/ufsrvcorelib.git "$d" && git -C "$d" sparse-checkout set dev/scripts && install -m 755 "$d/dev/scripts/cpu_affinity_verify.sh" cpu_affinity_verify.sh