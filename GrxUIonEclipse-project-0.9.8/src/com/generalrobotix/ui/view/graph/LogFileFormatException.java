/*
 * Copyright (c) 2008, AIST, the University of Tokyo and General Robotix Inc.
 * All rights reserved. This program is made available under the terms of the
 * Eclipse Public License v1.0 which accompanies this distribution, and is
 * available at http://www.eclipse.org/legal/epl-v10.html
 * Contributors:
 * General Robotix Inc.
 * National Institute of Advanced Industrial Science and Technology (AIST) 
 */
/**
 * LogFileFormatException.java
 *
 * @author  Kernel Co.,Ltd.
 * @version 1.0 (2001/3/1)
 */
package com.generalrobotix.ui.view.graph;

public class LogFileFormatException extends Exception {
    /**
     * コンストラクタ
     *
     * @param   String str    詳細
     */
    public LogFileFormatException() {
        super();
    }

    public LogFileFormatException(String str) {
        super(str);
    }
}
