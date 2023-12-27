#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>

const uint8_t M3_adelante = 0x04; // OCOA0
const uint8_t M3_atras = 0x01;
const uint8_t M4_adelante = 0x02; // OCOB0
const uint8_t M4_atras = 0x80;

const uint8_t velocidades[]={170,170,180,180};
volatile uint8_t velocidad;

volatile uint8_t status_timer1;
const uint8_t AUMENTAR_VELOCIDAD = 1;
const uint8_t FINALIZAR = 2;

volatile uint8_t tipo_de_linea;
const uint8_t NEGRA = 0;
const uint8_t BLANCA = 1;
volatile uint8_t lectura_pinc;

// CONFIGURACIONES
void configurar_puertos(void){
    DDRB = 0xFF; // Todo B como salida
    DDRD = 0xFF; // Todo D como salida
    DDRC = 0x01; // Todo C como entrada, Sensores

    return;
}

void configurar_interrupciones(void){
    PCICR = (1 << PCIE1);
    PCMSK1 = (1 << PCINT9) | (1 << PCINT10) | (1 << PCINT11) | (1 << PCINT12) | (1 << PCINT13) // Sensores
    TIMSK1 = (1 << OCIE1A);
    sei();

    return;
}

void config_motor(uint8_t motor) {
    PORTD &= 0x7F; // D7 en 0
    PORTB &= 0xEF;

    for(uint8_t i = 0; i < 8; i++) {
        PORTD &= 0xEF;
        if((motor & (1 << i)) == 0) {
            PORTB &= 0xFE;
        }
        else {
            PORTB |= 0x01;
        }

        PORTD |= 0x10;
        }

    PORTB |= 0x10;

    return;
}

// FUNCIONES DE TIMER
void apagar_timer0(void){
    TCCR0A = 0;
    TCCR0B = 0;

    return;
}

void prender_timer0(void){
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00); //Fast PWM ; Clear OC0A/OC0B on compare match, set OC0A at BOTTOM, (non-inverting mode);
    TCCR0B = (1 << CS00);

    return;
}

void apagar_timer1(void){
    TCCR1B = 0;

    return;
}

void prender_timer1(uint8_t tiempo_timer1){ // Prendemos un timer segun la cantidad de tiempo que nos pasan como parametro
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);

    if(tiempo_timer1 == 2) { // 2seg
        OCR1A = 31250;
        status_timer1 = FINALIZAR;
    }
    if(tiempo_timer1 == 1) { // 1seg
        OCR1A = 15625;
        status_timer1 = AUMENTAR_VELOCIDAD;
    }
    if(tiempo_timer1 == 3) { // 0.3seg
        OCR1A = 5200;
        status_timer1 = AUMENTAR_VELOCIDAD;
    }

    return;
}

 // FUNCIONES DE VELOCIDAD
void doblar_der(void) {
    OCR0A = 170;
    OCR0B = 135;
    return;
}

void doblar_izq(void) {
    OCR0A = 135;
    OCR0B = 170;
    return;
}

void doblar_der_f(void) {
    OCR0A = 180;
    OCR0B = 120;
    return;
}

void doblar_izq_f(void) {
    OCR0A = 120;
    OCR0B = 180;
    return;
}

void ir_recto(uint8_t velocidad) {
    prender_timer1(3);
    OCR0A = velocidad;
    OCR0B = velocidad;
    return;
}
void doblar_90_der(void) {
    OCR0A = 185;
    OCR0B = 0;
    return;
}
void doblar_90_izq(void) {
    OCR0A = 0;
    OCR0B = 185;
    return;
}

// INTERRUPCIONES
ISR(PCINT1_vect) {
    _delay_ms(50);

    if(tipo_de_linea == BLANCA) {
        // invertir PINC
        lectura_pinc = PINC ^ 0x3E;
    }

    if(tipo_de_linea == NEGRA) {
        lectura_pinc = PINC;
    }

    apagar_timer1();

    if (lectura_pinc == 0x3E) {
        prender_timer1(2);
        return;
    }

    prender_timer0();

    // 90 derecha
    if (lectura_pinc == 0x30) {
        doblar_90_der();
        return;
    }
    // 90 izquierda
    if (lectura_pinc == 0x06) {
        doblar_90_izq();
        return;
    }
    // Doblar derecha fuerte
    if (lectura_pinc == 0x32) {
        doblar_der_f();
        return;
    }
    // Doblar izquierda fuerte
        if (lectura_pinc == 0x26) {
        doblar_izq_f();
        return;
    }
    // Doblar derecha
        if (lectura_pinc == 0x3A){
        doblar_der();
        return;
    }
    // Doblar izquierda
        if (lectura_pinc == 0x2E) {
        doblar_izq();
        return;
    }
    // Ir recto
    if (lectura_pinc == 0x36) {
        velocidad = 0;
        ir_recto(velocidades[0]);
        return;
    }
}

ISR(TIMER1_COMPA_vect) {
    if(status_timer1 == AUMENTAR_VELOCIDAD) {
        if(velocidad == 3) {
            return;
        }
        velocidad += 1;
        ir_recto(velocidades[velocidad]);
        return;
    }

    if(status_timer1 == FINALIZAR) {
        config_motor(0x00);
        apagar_timer0();
        apagar_timer1();
        return;
    }

    return;
}

int main(void){
    _delay_ms(50000); // Delay inicial de 5 segundos
    configurar_puertos();

    if( (PINC & 0x22) == 0x22) { // Deteccion del tipo de pista
        // Negra
        tipo_de_linea = NEGRA;
    }
    else {
        // Blanco
        tipo_de_linea = BLANCA;
    }

    configurar_interrupciones();
    velocidad = 0;
    config_motor(0x06);
    prender_timer0();
    OCR0A = 120;
    OCR0B = 120;
    while(1){
    }
    return 0;
}
