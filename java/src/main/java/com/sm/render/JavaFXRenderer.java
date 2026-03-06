package com.sm.render;

import com.sm.Client;
import com.sm.protobufs.Pong;

import javafx.application.Platform;
import javafx.scene.Scene;
import javafx.scene.paint.Color;
import javafx.scene.shape.Rectangle;
import javafx.scene.layout.Pane;
import javafx.stage.Stage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import static com.sm.Constants.*;

public class JavaFXRenderer implements Renderer {

    private static final Logger logger = LoggerFactory.getLogger(JavaFXRenderer.class);

    private Stage stage;
    private final Pane gamePane = new Pane();
    private final Rectangle ballObject = new Rectangle();
    private final Rectangle[] paddleObjects = new Rectangle[2];

    private final Client client;

    public JavaFXRenderer(Client clientInstance) {
        client = clientInstance;
    }

    @Override
    public void setup() {
        Platform.startup(() -> {
            stage = new Stage();

            gamePane.setPrefSize(1280, 720);

            ballObject.setFill(Color.WHITE);

            setRelativeBounds(ballObject, 0.5f, 0.5f, MULTI_PONG_BALL_WIDTH, MULTI_PONG_BALL_HEIGHT);

            gamePane.getChildren().add(ballObject);

            for (int x = 0; x < 2; x++) {
                paddleObjects[x] = new Rectangle();
                paddleObjects[x].setFill(Color.WHITE);

                float horizontalPosition = x == 0 ? MULTI_PONG_PADDLE_HORIZONTAL_PADDING : 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING;
                setRelativeBounds(paddleObjects[x], horizontalPosition, 0.5f, MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT);

                gamePane.getChildren().add(paddleObjects[x]);
            }

            Scene scene = new Scene(gamePane, 1280, 720, Color.rgb(40, 40, 40));

            scene.setOnKeyPressed(e -> {
                switch (e.getCode()) {
                    case UP -> client.sendMovement(Pong.Direction.UP);
                    case DOWN -> client.sendMovement(Pong.Direction.DOWN);
                    case F -> toggleFullscreen();
                }
            });

            scene.setOnKeyReleased(e -> {
                switch (e.getCode()) {
                    case UP, DOWN -> client.sendMovement(Pong.Direction.STOP);
                }
            });

            stage.setScene(scene);
            stage.setTitle("Pong");
            stage.show();
        });
    }

    @Override
    public void toggleFullscreen() {
        Platform.runLater(() -> stage.setFullScreen(!stage.isFullScreen()));
    }

    @Override
    public void updateState(Pong.State state) {
        Platform.runLater(() -> {
            placeRelatively(ballObject, state.getBall().getX(), state.getBall().getY());
            placeRelatively(paddleObjects[0], MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.getPlayer1().getPaddleLocation());
            placeRelatively(paddleObjects[1], 1 - MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.getPlayer2().getPaddleLocation());
        });
    }

    @Override
    public void renderLoop() {
        logger.warn("This renderer uses retained mode rendering - exiting render loop");
    }

    @Override
    public void cleanup() {
        Platform.runLater(() -> {
            if (stage != null) stage.close();
        });
    }

    void setRelativeBounds(Rectangle object, float x, float y, float width, float height) {
        resizeRelatively(object, width, height);
        placeRelatively(object, x, y);
    }

    void resizeRelatively(Rectangle object, float width, float height) {
        object.widthProperty().bind(gamePane.widthProperty().multiply(width));
        object.heightProperty().bind(gamePane.heightProperty().multiply(height));
    }

    void placeRelatively(Rectangle object, float x, float y) {
        object.layoutXProperty().bind(gamePane.widthProperty().multiply(x).subtract(object.widthProperty().divide(2)));
        object.layoutYProperty().bind(gamePane.heightProperty().multiply(y).subtract(object.heightProperty().divide(2)));
    }
}
