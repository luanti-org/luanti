__worldname_list() {
  local string output
  string=$(luanti --worldlist name)

  IFS=$'\n' output=($string)
  output=("${output[@]:1}")
  output=("${output[@]#$'\t'}")
  printf '%s\n' "${output[@]}"
}

_luanti() {
  local cur prev opts file_opts unknown_val_opts color_values worldlist_values

  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"

  opts="--address --color --config --console --debugger --gameid --go --help --info --logfile --map-dir --migrate --migrate-auth --migrate-mod-storage --migrate-players --name --password --password-file --port --quiet --random-input --recompress --run-benchmarks --run-unittests --server --terminal --test-module --trace --verbose --version --world --worldlist --worldname"
  file_opts="--config --logfile --map-dir --password-file --world"
  unknown_val_opts="--address --migrate --migrate-auth --migrate-mod-storage --migrate-players --name --password --port --test-module"
  color_values="always auto never"
  worldlist_values="both name path"


  if [[ "$prev" == "--color" ]]; then
    COMPREPLY=($(compgen -W "$color_values" -- "$cur"))
  elif [[ "$prev" == "--worldlist" ]]; then
    COMPREPLY=($(compgen -W "$worldlist_values" -- "$cur"))
  elif [[ "$prev" == "--gameid" ]]; then
    COMPREPLY=($(compgen -W "list $(luanti --gameid list)" -- "$cur"))
  elif [[ "$prev" == "--worldname" ]]; then
    COMPREPLY=($(compgen -W "$(__worldname_list)" -- "$cur"))
  elif [[ " ${file_opts[*]} " == *" ${prev} "* ]]; then
    _comp_compgen_filedir
  elif [[ " ${unknown_val_opts[*]} " == *" ${prev} "* ]]; then
    :
  else
    COMPREPLY=($(compgen -W "$opts" -- "$cur"))
  fi
}

complete -F _luanti luanti
