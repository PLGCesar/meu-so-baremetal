#include <stddef.h>
#include <stdint.h>

// Endereço de memória da memória de vídeo VGA Text Mode
volatile uint16_t* const VGA_BUFFER = (uint16_t*)0xB8000;

void kernel_main(void) {
    const char* mensagem = "SISTEMA OPERACIONAL BARE-METAL VIVO NO DOCKER!";
    
    // Cor: Texto Branco (0x0F) com fundo Preto (0x00)
    uint16_t cor = 0x0F00; 

    // Escreve caractere por caractere direto no hardware de vídeo
    for (size_t i = 0; mensagem[i] != '\0'; i++) {
        VGA_BUFFER[i] = (uint16_t)mensagem[i] | cor;
    }
}
