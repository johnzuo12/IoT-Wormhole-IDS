/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

package se.sics.cooja.mspmote.interfaces;

import java.text.NumberFormat;
import java.util.*;
import javax.swing.*;
import org.apache.log4j.Logger;
import org.jdom.Element;

import se.sics.cooja.*;
import se.sics.cooja.Mote;
import se.sics.cooja.interfaces.Position;
import se.sics.cooja.mspmote.MspMote;
import se.sics.cooja.mspmote.MspMoteMemory;
import se.sics.mspsim.core.Memory;
import se.sics.mspsim.core.MemoryMonitor;

/**
 * Mote 3D position.
 *
 * <p>
 * This observable notifies when the position is changed.
 *
 * @author Fredrik Osterlind
 */
@ClassDescription("MspPosition")
public class MspPosition extends Position {
  private static Logger logger = Logger.getLogger(MspPosition.class);
  
  private MspMote mote = null;
  private MspMoteMemory moteMem = null;
  
  private double[] coords = new double[3];
  
  private boolean writeFlashHeader = true;
  private MemoryMonitor memoryMonitorX;
  private MemoryMonitor memoryMonitorY;
  private MemoryMonitor memoryMonitorZ;
  int moteLocX = -1;
  int moteLocY = -1;
  int moteLocZ = -1;

  /**
   * Creates a position for given mote with coordinates (x=0, y=0, z=0).
   *
   * @param mote
   *          Led's mote.
   * @see Mote
   * @see se.sics.cooja.MoteInterfaceHandler
   */
  public MspPosition(Mote mote) {
	
	super(mote);
    
	this.mote = (MspMote) mote;
    this.moteMem = (MspMoteMemory) mote.getMemory();
    coords[0] = 0.0f;
    coords[1] = 0.0f;
    coords[2] = 0.0f;
  }

  /**
   * Set position to (x,y,z).
   *
   * @param x New X coordinate
   * @param y New Y coordinate
   * @param z New Z coordinate
   */
  public void setCoordinates(double x, double y, double z) {
    coords[0] = x;
    coords[1] = y;
    coords[2] = z;

    moteLocX  = (int) x;
    moteLocY  = (int) y;
    moteLocZ  = (int) z;

	System.out.println("Setting Node the coordinates X, Y --> \n" + moteLocX + "" + moteLocY);
    
    if (moteMem.variableExists("node_loc_x")) {
		moteMem.setIntValueOf("node_loc_x", moteLocX);
		moteMem.setIntValueOf("node_loc_y", moteLocY);
		moteMem.setIntValueOf("node_loc_z", moteLocZ);

		if (writeFlashHeader) {
			/* Write to external flash */
			
			SkyFlash flash = mote.getInterfaces().getInterfaceOfType(SkyFlash.class);
			if (flash != null) {							
				flash.writeIDheader(moteLocX);
			}
			writeFlashHeader = false;
		}

	}
	if (memoryMonitorX == null && memoryMonitorY == null && memoryMonitorZ == null) {
	    memoryMonitorX = new MemoryMonitor.Adapter() {
	        @Override
	        public void notifyWriteAfter(int dstAddress, int data, Memory.AccessMode mode) {
	            byte[] id = new byte[2];
	            id[0] = (byte) (moteLocX & 0xff);
	            id[1] = (byte) ((moteLocX >> 8) & 0xff);
	            moteMem.setMemorySegment(dstAddress & ~1, id);	            	         
	        }

	    };    
	    
	    memoryMonitorY = new MemoryMonitor.Adapter() {
	        @Override
	        public void notifyWriteAfter(int dstAddress, int data, Memory.AccessMode mode) {
	            byte[] id = new byte[2];
	            id[0] = (byte) (moteLocY & 0xff);
	            id[1] = (byte) ((moteLocY >> 8) & 0xff);
	            moteMem.setMemorySegment(dstAddress & ~1, id);	            	         
	        }

	    };
	    
	    memoryMonitorZ = new MemoryMonitor.Adapter() {
	        @Override
	        public void notifyWriteAfter(int dstAddress, int data, Memory.AccessMode mode) {
	            byte[] id = new byte[2];
	            id[0] = (byte) (moteLocZ & 0xff);
	            id[1] = (byte) ((moteLocZ >> 8) & 0xff);
	            moteMem.setMemorySegment(dstAddress & ~1, id);	            	         
	        }

	    };
	    addMonitor("node_loc_x", memoryMonitorX);
	    addMonitor("node_loc_y", memoryMonitorY);  
	    addMonitor("node_loc_z", memoryMonitorZ);  
	}
    this.setChanged();
    this.notifyObservers(mote);
  }

private void addMonitor(String variable, MemoryMonitor monitor) {
	    if (moteMem.variableExists(variable)) {
	        int address = moteMem.getVariableAddress(variable);
	        if ((address & 1) != 0) {
	            // Variable can not be a word - must be a byte
	        } else {
	            mote.getCPU().addWatchPoint(address, monitor);
	            mote.getCPU().addWatchPoint(address + 1, monitor);
	        }
	    }
	}

      private void removeMonitor(String variable, MemoryMonitor monitor) {
          if (moteMem.variableExists(variable)) {
              int address = moteMem.getVariableAddress(variable);
              mote.getCPU().removeWatchPoint(address, monitor);
              mote.getCPU().removeWatchPoint(address + 1, monitor);
          }
      }

  /**
   * @return X coordinate
   */
  public double getXCoordinate() {
    return coords[0];
  }

  /**
   * @return Y coordinate
   */
  public double getYCoordinate() {
    return coords[1];
  }

  /**
   * @return Z coordinate
   */
  public double getZCoordinate() {
    return coords[2];
  }

  /**
   * Calculates distance from this position to given position.
   *
   * @param pos Compared position
   * @return Distance
   */
  public double getDistanceTo(Position pos) {
    return Math.sqrt(Math.abs(coords[0] - pos.getXCoordinate())
        * Math.abs(coords[0] - pos.getXCoordinate())
        + Math.abs(coords[1] - pos.getYCoordinate())
        * Math.abs(coords[1] - pos.getYCoordinate())
        + Math.abs(coords[2] - pos.getZCoordinate())
        * Math.abs(coords[2] - pos.getZCoordinate()));
  }

  /**
   * Calculates distance from associated mote to another mote.
   *
   * @param m Another mote
   * @return Distance
   */
  public double getDistanceTo(Mote m) {
    return getDistanceTo(m.getInterfaces().getPosition());
  }

  public JPanel getInterfaceVisualizer() {
    JPanel panel = new JPanel();
    panel.setLayout(new BoxLayout(panel, BoxLayout.Y_AXIS));
    final NumberFormat form = NumberFormat.getNumberInstance();

    final JLabel positionLabel = new JLabel();
    positionLabel.setText("x=" + form.format(getXCoordinate()) + " "
        + "y=" + form.format(getYCoordinate()) + " "
        + "z=" + form.format(getZCoordinate()));

    panel.add(positionLabel);

    Observer observer;
    this.addObserver(observer = new Observer() {
      public void update(Observable obs, Object obj) {
        positionLabel.setText("x=" + form.format(getXCoordinate()) + " "
            + "y=" + form.format(getYCoordinate()) + " "
            + "z=" + form.format(getZCoordinate()));
      }
    });

    // Saving observer reference for releaseInterfaceVisualizer
    panel.putClientProperty("intf_obs", observer);

    return panel;
  }

  public void releaseInterfaceVisualizer(JPanel panel) {
    Observer observer = (Observer) panel.getClientProperty("intf_obs");
    if (observer == null) {
      logger.fatal("Error when releasing panel, observer is null");
      return;
    }

    this.deleteObserver(observer);
  }

  public Collection<Element> getConfigXML() {
    Vector<Element> config = new Vector<Element>();
    Element element;

    // X coordinate
    element = new Element("x");
    element.setText(Double.toString(getXCoordinate()));
    config.add(element);

    // Y coordinate
    element = new Element("y");
    element.setText(Double.toString(getYCoordinate()));
    config.add(element);

    // Z coordinate
    element = new Element("z");
    element.setText(Double.toString(getZCoordinate()));
    config.add(element);

    return config;
  }

  public void setConfigXML(Collection<Element> configXML, boolean visAvailable) {
    double x = 0, y = 0, z = 0;

    for (Element element : configXML) {
      if (element.getName().equals("x")) {
        x = Double.parseDouble(element.getText());
      }

      if (element.getName().equals("y")) {
        y = Double.parseDouble(element.getText());
      }

      if (element.getName().equals("z")) {
        z = Double.parseDouble(element.getText());
      }
    }

    setCoordinates(x, y, z);
  }

}
