# @file test_model_builder.gdb
# @brief GDB-скрипт для отладки и проверки model_builder.
# Запускает model_builder на тестовом корпусе и проверяет код возврата,
# а также наличие выходных файлов.

set pagination off
set confirm off

# Устанавливаем аргументы: входная папка tests/corpus, выходная output/test
run tests/corpus output/test

# Проверяем код возврата (должен быть 0)
if $_exitcode != 0
  printf "FAIL: model_builder returned code %d\n", $_exitcode
  quit 1
else
  printf "PASS: model_builder exited with code 0\n"
end

# Проверяем существование model.bin
shell if not exist output\test\model.bin ( exit 1 )
if $_exitcode != 0
  printf "FAIL: model.bin not found\n"
  quit 1
else
  printf "PASS: model.bin exists\n"
end

# Проверяем существование vocab.txt
shell if not exist output\test\vocab.txt ( exit 1 )
if $_exitcode != 0
  printf "FAIL: vocab.txt not found\n"
  quit 1
else
  printf "PASS: vocab.txt exists\n"
end

# Дополнительно: проверяем размер model.bin (должен быть не пустым)
shell for %%A in (output\test\model.bin) do if %%~zA==0 exit 1
if $_exitcode != 0
  printf "FAIL: model.bin is empty\n"
  quit 1
else
  printf "PASS: model.bin is not empty\n"
end

quit 0