#include <ICMPPing.h>
#include <SPI.h>
#include <Ethernet.h>
#include <MFRC522.h>
#include <TimeLib.h>
#include <EEPROM.h>

//Variáveis de configuração
#define SS_PIN 53 //Pino Placa RFID
#define RST_PIN 41 //Pino Placa RFID
MFRC522 mfrc522(SS_PIN, RST_PIN); //Cria instancia MFRC522(RFID)

byte mac[] = {0x90, 0xA2, 0xDA, 0x5E, 0x8B, 0xE1}; //Define MAC da Placa
String ipArduino = "192,168,90,159"; //Define IP da placa
char serverName[] = "13.65.249.119";  //Define IP para Enviar de Informações
String ipServidor = "13.65.249.119";  //Define IP para Enviar de Informações
int serverPort = 57772; //Define Porta para Enviar de Informações


IPAddress serverHost(8,8,8,8);  //Define IP para Ping
IPAddress subnet(255,255,255,0); //Deine Mascara da placa
IPAddress gateway(192,168,90,1); //Define Gateway da placa
IPAddress DNS(8,8,8,8); //Define DNS da Placa 

ICMPPing ping(3, (uint16_t)random(0, 255)); //Define parametros para ping

EthernetServer server(80); // Define porta de Servidor para receber comandos(Ligar/Desligar Rele)
EthernetClient client; //Inicia Ethernet para Enviar informações para Banco de Dados
EthernetClient cliente; //Inicia Ethernet para receber comandos (Ligar/Desligar Rele)

// Variáveis
int totalCount = 0; //Contador WS
char pageAdd[64]; //Pagina WS
char buffer [256]; //Buffer WS
String content; //RFID
int rele = 22;  //Saída Rele
int releVcc = 23; //Energia Rele
String readString; //Leitura de Requisição
int porta[101]; //Array de Status das portas
int delayPing = 10; //Tempo(Segundos) em que será feito um ping para conferir internet
int enderecoReleEEPROM = 1020; //Endereço de memoria onde está gravado o status do Rele
int statusReleEEPROM; //Status do Rele gravado na Memoria
int atualizaPortas = 0; //Necessidade de mandar requisição para atualizar as portas no bando de dados
int atualizaRFID = 0; //Necessidade de mandar requisição para atualizar o RFID no bando de dados
String RFIDValido; //Ultimo RFID que foi mandado para o Banco de dados
String RFIDAntes; //Ultimo RFID que foi lido
String RFIDAntesAntes; //Penultimo RFID que foi lido

String splitString(String data, char separator, int index) //Separa String em Blocos(Tipo Split)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

int IP1Arduino = splitString(ipArduino, ',', 0).toInt();
int IP2Arduino = splitString(ipArduino, ',', 1).toInt();
int IP3Arduino = splitString(ipArduino, ',', 2).toInt();
int IP4Arduino = splitString(ipArduino, ',', 3).toInt();
IPAddress ip(IP1Arduino,IP2Arduino,IP3Arduino,IP4Arduino); //Define IP para Receber Comandos (Ligar/Desligar Rele)


int IP1Servidor = splitString(ipServidor, '.', 0).toInt();
int IP2Servidor = splitString(ipServidor, '.', 1).toInt();
int IP3Servidor = splitString(ipServidor, '.', 2).toInt();
int IP4Servidor = splitString(ipServidor, '.', 3).toInt();
IPAddress servidor(IP1Servidor,IP2Servidor,IP3Servidor,IP4Servidor); // Define IP para Enviar de Informações


void setup() 
{
  pinMode(rele, OUTPUT);
  digitalWrite(rele, HIGH);
  
  pinMode(releVcc, OUTPUT);
  digitalWrite(releVcc, HIGH);
  
  Serial.begin(9600);   // Inicia Comunicação Serial

  SPI.begin();      // Inicia SPI bus
  mfrc522.PCD_Init();   // Inicia Leitor RF MFRC522
  Serial.println("Aproxime o Cartao do Leitor...");
  Serial.println();

  Ethernet.begin(mac,ip,subnet,gateway,DNS);
  
  Serial.print("O Server esta em: ");
  Serial.println(Ethernet.localIP());
}



void loop() 
{
    testePingRede();  //Entra no ping para verificar rede
    iniciaRele(); //Inicia Rele com o Ultimo Status

    if((porta[2] != digitalRead(2)) || (porta[4] != digitalRead(4)) || (porta[5] != digitalRead(5)) || (porta[6] != digitalRead(6))  || (porta[100] != digitalRead(rele))){
      atualizaPortas = 1; //Envia o status das portas para o WebService por ser diferente do status anterior
    }else{
      atualizaPortas = 0; //Não envia o status das portas para o WebService
    }

    if (digitalRead(4) == 0){// Se Porta Automatico está ligada
      if(porta[3] != digitalRead(2)){
        atualizaPortas = 1; //Envia o status das portas para o WebService por ser diferente do status anterior
      }      
    }else{
      if(porta[3] != digitalRead(3)){
        atualizaPortas = 1; //Envia o status das portas para o WebService por ser diferente do status anterior
      }   
    }
    
    //Seta status das portas no Array
    porta[2] = digitalRead(2); //Regua
    // Verifica se Porta 4(Automatico) está ativa 
    if (digitalRead(4) == 0){
         porta[3] = {digitalRead(2)};// Seta porta 3 como leitura da porta 2
    }else{
         porta[3] = {digitalRead(3)};// Seta porta 3 como leitura da porta 3
    }
    porta[4] = digitalRead(4); //Automatico
    porta[5] = digitalRead(5); //Serra
    porta[6] = digitalRead(6); //Alimentacao
    porta[100] = digitalRead(rele); //Rele

    
    String content; // Variável que irá receber conteúdo
    
    // Busca novos cartoes
    if (!mfrc522.PICC_IsNewCardPresent()) 
    {
      content = ""; //cartão não encontrado, Coloca contaudo como Nulo
      goto depoisRFID;
    }
    
    // Seleciona um dos cartoes
    if (!mfrc522.PICC_ReadCardSerial()) 
    {
      content = ""; //cartão não encontrado, Coloca contaudo como Nulo
      goto depoisRFID;
    }
    
    //Cria o ID do Cartao
    byte letter;
    for (byte i = 0; i < mfrc522.uid.size; i++) 
    {
       content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
       content.concat(String(mfrc522.uid.uidByte[i], HEX));
    }

    //Grava o UID em Caixa Alta
    content.toUpperCase();
    
    
    //GOTO
    depoisRFID:

    //Variaveis para Gap entre Leitura do cartao
    RFIDAntesAntes = RFIDAntes;
    RFIDAntes = content;

    //Verificacao de Leitura do cartao
    //
    if((content == NULL) && (RFIDAntes == NULL) && (RFIDAntesAntes == NULL)){
      content = ""; // Se for lido 3 vezes em sequencia o cartão como nulo, é setado o conteudo como nulo
    }else{
      if(RFIDAntes != NULL){
        content = RFIDAntes; //Seta o Conteúdo como o ultimo RFID que não foi nulo
      }
      if(RFIDAntesAntes != NULL){
        content = RFIDAntesAntes; //Seta o Conteúdo como o penultimo RFID que não foi nulo
      }
    }

    //Validacao se precisa atualizar o RFID no Cache
    if(RFIDValido != content){
      atualizaRFID = 1;  //Envia o RFID para o WebService por ser diferente do status anterior
    }else{
      atualizaRFID = 0;  //Não envia o RFID para o WebService
    }

    
    //Caso uma porta for mudada, o algoritmo entrará nesta condição.
    if(atualizaPortas == 1){
      String regua = String(porta[2]);
      String corte = String(porta[3]);
      String automatico = String(porta[4]);
      String serra = String(porta[5]);
      String alimentacao = String(porta[6]);  
      String portaRele = String(porta[100]);  

      //Cria URL para o WS
      String url = "/csp/arduino/Portas/"+ipArduino+"/"+regua+"/"+corte+"/"+automatico+"/"+serra+"/"+alimentacao+"/"+portaRele;
      const char * urlFinal = url.c_str();
      Serial.println(urlFinal);

      //Adiciona na conf
      sprintf(pageAdd,urlFinal,totalCount);
      //Manda para o WebService
      Serial.println("1 Tentativa");
      int resMandaPortas = getPage(servidor, serverPort, pageAdd);
      if(resMandaPortas == 0){
        Serial.println("2 Tentativa");
        int resMandaPortas2 = getPage(servidor, serverPort, pageAdd);
        if(resMandaPortas2 == 0){
          Serial.println("3 Tentativa");
          int resMandaPortas3 = getPage(servidor, serverPort, pageAdd);
          if(resMandaPortas3 == 0){
            Serial.println("4 Tentativa");
            int resMandaPortas4 = getPage(servidor, serverPort, pageAdd);
            if(resMandaPortas4 == 0){
              getPage(servidor, serverPort, pageAdd);
              resetArduino();
            }
          }
        }
      } 
      Serial.println("Enviado Portas!!!");  
    }
    
    //Caso o RFID Muda entrará nesta condição.
    if(atualizaRFID == 1){
      RFIDValido = content;
      if(content == NULL){
        content = "0";  //Seta Variável como 0 para que possa ser passada para o WebService e gravada no Banco de dados
      }else{
        content = espacoParaUnderLine(content); //Coloca Underlines '_' no lugar de espaços, para que o WebService possa enviar para o Banco de dados
      }
      //Cria URL para o WS
      String url = "/csp/arduino/RFID/"+ipArduino+"/"+content;
      const char * urlFinal = url.c_str();
      Serial.println(urlFinal);
  
      //Adiciona na conf
      sprintf(pageAdd,urlFinal,totalCount);
      //Manda para o WebService
      Serial.println("1 Tentativa");
      int resMandaRFID = getPage(servidor, serverPort, pageAdd);
      if(resMandaRFID == 0){
        delay(500);
        Serial.println("2 Tentativa");
        int resMandaRFID2 = getPage(servidor, serverPort, pageAdd);
        if(resMandaRFID2 == 0){
          delay(500);
          Serial.println("3 Tentativa");
          int resMandaRFID3 = getPage(servidor, serverPort, pageAdd);
          if(resMandaRFID3 == 0){
            delay(500);
            Serial.println("4 Tentativa");
            int resMandaRFID4 = getPage(servidor, serverPort, pageAdd);
            if(resMandaRFID4 == 0){
              delay(500);
              getPage(servidor, serverPort, pageAdd);
              resetArduino();
            }
          }
        }
      } 
      Serial.println("Enviado RFID !!!");  
     }
     iniciaEthernet(content);
 }


void iniciaRele(){
    statusReleEEPROM = EEPROM.read(enderecoReleEEPROM); //Pega Situacao do Rele que foi gravada na memoria
    if(statusReleEEPROM == 10){
      if(digitalRead(rele)!= 1){
        digitalWrite(rele, HIGH); //Se Situacao Gravada foi 10 e Situacao atual do Rele for diferente de 1, ativar o pino do rele
      }
    }else{
      if(digitalRead(rele)!= 0){
        digitalWrite(rele, LOW); //Se Situacao Gravada foi 10 e Situacao atual do Rele for diferente de 0, Zerar o pino do rele
      }  
    }  
}

byte getPage(IPAddress ipBuf,int thisPort, char *page)  //Enviar Requisição para o Caché
{
  int inChar;
  char outBuf[128];

  Serial.print(F("connecting..."));

  if(client.connect(ipBuf,thisPort) == 1)
  {
    Serial.println(F("connected"));

    sprintf(outBuf,"GET %s HTTP/1.1",page);
    client.println(outBuf);
    sprintf(outBuf,"Host: %s",serverName);
    client.println(outBuf);
    client.println(F("Connection: close\r\n"));
  }
  else
  {
    Serial.println(F("failed"));
    return 0;
  }

  // connectLoop controls the hardware fail timeout
  int connectLoop = 0;

  while(client.connected())
  {
    while(client.available())
    {
      inChar = client.read();
      //Serial.write(inChar);
      // set connectLoop to zero if a packet arrives
      connectLoop = 0;
    }

    connectLoop++;

    // if more than 5000 milliseconds since the last packet
    if(connectLoop > 5000)
    {
      // then close the connection from this end.
      Serial.println();
      Serial.println(F("Timeout"));
      client.stop();
    }
    // this is a delay for the connectLoop timing
    delay(1);
  }

  Serial.println();

  Serial.println(F("disconnecting."));
  // close client end
  client.stop();

  return 1;
}

void testePingRede(){ //Verificar se há conexão com a Internet
      if(delayPing == second()){
      if(delayPing != 55){
        delayPing = delayPing+5;
      }else{
        delayPing=5;
      }
      ICMPEchoReply echoReply = ping(serverHost, 1);
      if (echoReply.status == SUCCESS){
        sprintf(buffer, "Comunicacao Ok, Status: %d", echoReply.status);
      }else if (echoReply.status == NO_RESPONSE){
        sprintf(buffer, "Sem Comunicacao, Status: %d", echoReply.status);
        if(digitalRead(5) == 0){
          Serial.println("Reinicia");
          resetArduino();
        }
      }else{
        sprintf(buffer, "Erro, Status: %d", echoReply.status);
        if(digitalRead(5) == 0){
          Serial.println("Reinicia");
          resetArduino();
        } 
      }
      Serial.println(buffer);

      Serial.println(delayPing);
    }
}

void iniciaEthernet(String content){  //Inicia Servidor de
   //Aguarda conexao do browser
  EthernetClient client = server.available();
  if (client) {
    Serial.println("novo cliente");
    Serial.println(content);
    // uma requisicao http request acaba em uma linha em branco
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //lê o HTTP request
        if (readString.length() < 100) {

          //grava caracteres na string 
          readString += c; 
          //Serial.print(c);
        } 
        
        //se o request HTTP acabou  
         if(readString.indexOf('?') >=0) { //don't send new page
          goto funcao;
          }else{

        // se chegou ao fim da linha(recebeu um caracter para nova linha) e a linha esta em branco, o request http acabou, então Resposta é enviada para o cliente
        if (c == 'n' && currentLineIsBlank) {
            client.println();
            break;
          }
          if (c == 'n') {
            // Se esta iniciando uma linha nova
            currentLineIsBlank = true;
          } 
          else if (c != 'r') {
            // Se ja tem um caractere na linha
            currentLineIsBlank = false;
          }
        }
        
        funcao:
        if (c == '\n') {

        if(readString.indexOf("status")>0){
          Serial.println("Resposta");
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();
          client.println("<XML>");
            client.print("<RFID>");
              client.print(content);//Grava o UID do Cartao
            client.print("</RFID>");
  
            //Gravação porta 2
            client.println("<porta>");
              client.print("<id>");
                client.print("2");//Grava a Porta
              client.print("</id>");
              client.print(" ");
              client.print("<status>");
                client.print(digitalRead(2));//Grava o status da porta como 1
              client.print("</status>");
            client.println("</porta>");
  
            //Gravação porta 3
            client.println("<porta>");
              client.print("<id>");
                client.print("3");//Grava a Porta
              client.print("</id>");
              client.print(" ");
              client.print("<status>");
                client.print(digitalRead(3));//Grava o status da porta como 1
              client.print("</status>");
            client.println("</porta>");
  
            //Gravação porta 4
            client.println("<porta>");
              client.print("<id>");
                client.print("4");//Grava a Porta
              client.print("</id>");
              client.print(" ");
              client.print("<status>");
                client.print(digitalRead(4));//Grava o status da porta como 1
              client.print("</status>");
            client.println("</porta>");
  
            //Gravação porta 5
            client.println("<porta>");
              client.print("<id>");
                client.print("5");//Grava a Porta
              client.print("</id>");
              client.print(" ");
              client.print("<status>");
                client.print(digitalRead(5));//Grava o status da porta como 1
              client.print("</status>");
            client.println("</porta>");
  
            //Gravação porta 6
            client.println("<porta>");
              client.print("<id>");
                client.print("6");//Grava a Porta
              client.print("</id>");
              client.print(" ");
              client.print("<status>");
                client.print(digitalRead(6));//Grava o status da porta como 1
              client.print("</status>");
            client.println("</porta>");
  
            client.println("<porta>");
              client.print("<id>");
                client.print("100");//Grava a Porta
              client.print("</id>");
              client.print(" ");
              client.print("<status>");
                client.print(digitalRead(rele));//Grava o status da porta como 1
              client.print("</status>");
            client.println("</porta>");
          client.println("</xml>");
        }  

          
          if(readString.indexOf("pisca") >0)//procura pelo parametro "pisca"
        {
          digitalWrite(rele, HIGH); 
          Serial.println("Led on");
          delay(1000);
          digitalWrite(rele, LOW);
          Serial.println("Led off");
        }
        if(readString.indexOf("reset") >0)//procura pelo parametro "reset"
        {
          resetArduino();
        }
        
        if(readString.indexOf("acende") >0)//procura pelo parametro "acende"
        {
          digitalWrite(rele, HIGH);// seta o Pino como 0
          Serial.println("Maquina Ligada");
          Serial.println();
          Serial.println("A:"+EEPROM.read(enderecoReleEEPROM));
          Serial.println();
          if(digitalRead(rele) == 1){
            if(EEPROM.read(enderecoReleEEPROM) != 10){
              EEPROM.write(enderecoReleEEPROM,10);
            } 
          }else{
            if(EEPROM.read(enderecoReleEEPROM) != 20){
              EEPROM.write(enderecoReleEEPROM,20);
            } 
          }
          Serial.println();
          Serial.println("N:"+EEPROM.read(enderecoReleEEPROM));
          Serial.println();
        }
        
        if(readString.indexOf("apaga") >0)//procura pelo parametro "apaga"
        {
          digitalWrite(rele, LOW);// seta o Pino como 1
          Serial.println("Maquina Desligada");
          Serial.println();
          Serial.println("A:"+EEPROM.read(enderecoReleEEPROM));
          Serial.println();
          if(digitalRead(rele) == 1){
            if(EEPROM.read(enderecoReleEEPROM) != 10){
              EEPROM.write(enderecoReleEEPROM,10);
            } 
          }else{
            if(EEPROM.read(enderecoReleEEPROM) != 20){
              EEPROM.write(enderecoReleEEPROM,20);
            } 
          }
          Serial.println();
          Serial.println("N:"+EEPROM.read(enderecoReleEEPROM));
          Serial.println();
        }
        //clearing string for next read
        readString="";
        }      
      }
    }
    // fecha a conexao:
    client.stop();
    }
}

void resetArduino(){
    Serial.println();
    Serial.println("A:"+EEPROM.read(enderecoReleEEPROM));
    if(digitalRead(rele) == 1){
      if(EEPROM.read(enderecoReleEEPROM) != 10){
        EEPROM.write(enderecoReleEEPROM,10);
      } 
    }else{
      if(EEPROM.read(enderecoReleEEPROM) != 20){
        EEPROM.write(enderecoReleEEPROM,20);
      } 
    }
    Serial.println("N:"+EEPROM.read(enderecoReleEEPROM));
    Serial.println();
    delay(1000);  
    int pin=8;
    pinMode(pin, OUTPUT);// seta o Pino como Saida
    digitalWrite(pin, LOW);// seta o Pino como 0
  }

String espacoParaUnderLine(String text) //Troca Underline '_' por Espaço ' '
{
    for(int i = 0; i < text.length(); i++)
    {
        if(text[i] == ' ')
            text[i] = '_';
    }
    return text;
}

