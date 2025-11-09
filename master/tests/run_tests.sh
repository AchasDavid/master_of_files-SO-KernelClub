#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "🔍 Ejecutando todos los tests..."

# Compilar todos los tests
make clean && make test

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Error al compilar los tests.${NC}"
    exit 1
fi

echo -e "${YELLOW}🧪 Compilación exitosa. Ejecutando binarios...${NC}"

# Ejecutar todos los binarios que empiecen con "test_" en subcarpetas
FAILURES=0
TOTAL=0

for test_bin in $(find . -type f -name 'test_*' -executable); do
    echo -e "${YELLOW}➡️ Ejecutando ${test_bin}${NC}"
    "$test_bin"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ ${test_bin} pasó correctamente.${NC}"
    else
        echo -e "${RED}❌ ${test_bin} falló.${NC}"
        FAILURES=$((FAILURES+1))
    fi
    TOTAL=$((TOTAL+1))
done

echo ""
echo "📊 Resumen:"
echo "Total de tests ejecutados: $TOTAL"
echo "Tests exitosos: $((TOTAL - FAILURES))"
echo "Tests fallidos: $FAILURES"

if [ $FAILURES -eq 0 ]; then
    echo -e "${GREEN}🎉 Todos los tests pasaron correctamente.${NC}"
else
    echo -e "${RED}⚠️ Algunos tests fallaron. Revisar logs.${NC}"
    exit 1
fi
