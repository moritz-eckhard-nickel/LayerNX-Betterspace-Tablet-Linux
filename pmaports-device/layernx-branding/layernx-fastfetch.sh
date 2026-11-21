# LayerNX-Tablet-Linux: fastfetch beim Oeffnen eines interaktiven Terminals (einmal pro Shell)
case $- in *i*) ;; *) return 0 2>/dev/null ;; esac
if [ -z "$LAYERNX_FETCHED" ] && command -v fastfetch >/dev/null 2>&1; then
	export LAYERNX_FETCHED=1
	fastfetch --config /usr/share/layernx/fastfetch.jsonc 2>/dev/null
fi
