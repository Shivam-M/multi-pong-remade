package com.sm.render;

import com.sm.Client;
import com.sm.protobufs.Pong;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;

import static com.sm.Constants.*;

public class SwingRenderer implements Renderer {

    private static final Logger logger = LoggerFactory.getLogger(SwingRenderer.class);

    private final JFrame windowFrame = new JFrame();
    private final JPanel gamePanel = new JPanel(null);
    private final JPanel ballObject = new JPanel(null);
    private final JPanel[] paddleObjects = new JPanel[2];
    private final Client client;

    public SwingRenderer(Client clientInstance) {
        client = clientInstance;
    }

    @Override
    public void setup() {
        windowFrame.setSize(1280, 720);
        windowFrame.setBackground(new Color(40, 40, 40));
        windowFrame.setTitle("Pong");

        gamePanel.setBackground(windowFrame.getBackground());
        gamePanel.setSize(windowFrame.getSize());

        ballObject.setBackground(Color.WHITE);

        setRelativeBounds(ballObject, 0.5f, 0.5f, MULTI_PONG_BALL_WIDTH, MULTI_PONG_BALL_HEIGHT);

        gamePanel.add(ballObject);

        for (int x = 0; x < 2; x++) {
            paddleObjects[x] = new JPanel(null);
            paddleObjects[x].setBackground(Color.WHITE);

            float horizontalPosition = x == 0 ? MULTI_PONG_PADDLE_HORIZONTAL_PADDING : 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING;
            setRelativeBounds(paddleObjects[x], horizontalPosition, 0.5f, MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT);

            gamePanel.add(paddleObjects[x]);
        }

        bindKey("F", this::toggleFullscreen);
        bindKey("UP", () -> client.sendMovement(Pong.Direction.UP));
        bindKey("DOWN", () -> client.sendMovement(Pong.Direction.DOWN));
        bindKey("released UP", () -> client.sendMovement(Pong.Direction.STOP));
        bindKey("released DOWN", () -> client.sendMovement(Pong.Direction.STOP));

        windowFrame.setContentPane(gamePanel);
        windowFrame.setVisible(true);
    }

    @Override
    public void toggleFullscreen() {
        GraphicsDevice device = GraphicsEnvironment.getLocalGraphicsEnvironment().getDefaultScreenDevice();
        if (device.getFullScreenWindow() == null) {
            device.setFullScreenWindow(windowFrame);
        } else {
            device.setFullScreenWindow(null);
        }
        gamePanel.setSize(windowFrame.getContentPane().getSize());
    }

    @Override
    public void updateState(Pong.State state) {
        placeRelatively(ballObject, state.getBall().getX(), state.getBall().getY());
        placeRelatively(paddleObjects[0], MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.getPlayer1().getPaddleLocation());
        placeRelatively(paddleObjects[1], 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.getPlayer2().getPaddleLocation());
    }

    @Override
    public void renderLoop() {
        logger.warn("This renderer uses retained mode rendering - exiting render loop");
    }

    @Override
    public void cleanup() {
        windowFrame.dispose();
    }

    // might be better to use a custom layout or grid bounds
    void setRelativeBounds(JPanel object, float x, float y, float width, float height) {
        resizeRelatively(object, width, height);
        placeRelatively(object, x, y);
    }

    void resizeRelatively(JPanel object, float width, float height) {
        object.setSize((int)(width * gamePanel.getWidth()), (int)(height * gamePanel.getHeight()));
    }

    void placeRelatively(JPanel object, float x, float y) {
        object.setLocation(
                (int)(x * gamePanel.getWidth()) - object.getWidth() / 2,
                (int)(y * gamePanel.getHeight()) - object.getHeight() / 2
        );
    }

    void bindKey(String key, Runnable action) {
        windowFrame.getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW).put(KeyStroke.getKeyStroke(key), key);
        windowFrame.getRootPane().getActionMap()
                .put(key, new AbstractAction() {
                    public void actionPerformed(ActionEvent e) { action.run(); }
                });
    }

}
