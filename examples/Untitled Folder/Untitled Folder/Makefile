DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
                        all: $(CONTIKI_PROJECT)

                        CONTIKI = ../..

                        WITH_UIP6=1
                        UIP_CONF_IPV6=1

                        CFLAGS+= -DUIP_CONF_IPV6_RPL -DUIP_CONF_IPV6 -DWITH_UIP6
			LDLIBS=-lm
                        ifdef PERIOD
                        CFLAGS=-DPERIOD=$(PERIOD)
                        endif


                        include $(CONTIKI)/Makefile.include
